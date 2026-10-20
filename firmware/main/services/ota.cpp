#include "ota.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

static const char* TAG = "ota";

namespace {

constexpr size_t BufferBytes = 32 * 1024;
constexpr size_t ProgressStep = 32 * 1024;

enum class Op : uint8_t { Begin, End, Abort };
struct Command {
	Op op;
	size_t size;
	uint8_t sha[32];
};

ota::Report report = nullptr;
QueueHandle_t commands = nullptr;
StreamBufferHandle_t stream = nullptr;
std::atomic<bool> running{ false };
std::atomic<bool> failure{ false };
std::atomic<size_t> expected{ 0 };
std::atomic<size_t> received{ 0 };

void say(const char* json) {
	ESP_LOGI(TAG, "%s", json);
	if (report) report(json);
}

void fail(const char* err) {
	failure = true;
	char buf[96];
	snprintf(buf, sizeof(buf), "{\"ok\":false,\"err\":\"%s\"}", err);
	say(buf);
}

void task(void*) {
	esp_ota_handle_t handle = 0;
	const esp_partition_t* target = nullptr;
	mbedtls_sha256_context sha;
	uint8_t want[32] = {};
	size_t written = 0, nextReport = ProgressStep;
	static uint8_t chunk[4096];

	auto stop = [&](bool abortWrite) {
		if (abortWrite && handle) esp_ota_abort(handle);
		handle = 0;
		running = false;
		mbedtls_sha256_free(&sha);
		xStreamBufferReset(stream);
	};

	while (true) {
		Command cmd;
		// While receiving, drain data and look for commands in between.
		const TickType_t wait = running ? 0 : portMAX_DELAY;
		if (xQueueReceive(commands, &cmd, wait) == pdTRUE) {
			switch (cmd.op) {
				case Op::Begin: {
					if (running) stop(true);
					target = esp_ota_get_next_update_partition(nullptr);
					if (!target || cmd.size == 0 || cmd.size > target->size) {
						fail(target ? "size" : "no slot");
						continue;
					}
					ESP_LOGI(TAG, "erasing %s for %u bytes", target->label, unsigned(cmd.size));
					if (esp_ota_begin(target, cmd.size, &handle) != ESP_OK) {
						handle = 0;
						fail("begin");
						continue;
					}
					mbedtls_sha256_init(&sha);
					mbedtls_sha256_starts(&sha, 0);
					memcpy(want, cmd.sha, sizeof(want));
					expected = cmd.size;
					received = 0;
					written = 0;
					nextReport = ProgressStep;
					xStreamBufferReset(stream);
					running = true;
					failure = false;
					say("{\"ok\":true,\"op\":\"begin\"}");
					break;
				}
				case Op::Abort:
					if (running) stop(true);
					say("{\"ok\":true,\"op\":\"abort\"}");
					break;
				case Op::End: {
					if (!running) {
						fail("not started");
						continue;
					}
					// Finish whatever is still buffered.
					while (written < expected) {
						const size_t n = xStreamBufferReceive(stream, chunk, sizeof(chunk), pdMS_TO_TICKS(2000));
						if (n == 0) break;
						if (esp_ota_write(handle, chunk, n) != ESP_OK) break;
						mbedtls_sha256_update(&sha, chunk, n);
						written += n;
					}
					if (written != expected) {
						stop(true);
						fail("short");
						continue;
					}
					uint8_t got[32];
					mbedtls_sha256_finish(&sha, got);
					if (memcmp(got, want, sizeof(got)) != 0) {
						stop(true);
						fail("sha256");
						continue;
					}
					say("{\"ok\":true,\"op\":\"verify\"}");
					if (esp_ota_end(handle) != ESP_OK) {
						handle = 0;
						running = false;
						fail("image");
						continue;
					}
					handle = 0;
					if (esp_ota_set_boot_partition(target) != ESP_OK) {
						running = false;
						fail("boot");
						continue;
					}
					say("{\"ok\":true,\"op\":\"end\"}");
					vTaskDelay(pdMS_TO_TICKS(1000));
					esp_restart();
				}
			}
			continue;
		}

		const size_t n = xStreamBufferReceive(stream, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
		if (n == 0) continue;
		if (written + n > expected) {
			stop(true);
			fail("overflow");
			continue;
		}
		if (esp_ota_write(handle, chunk, n) != ESP_OK) {
			stop(true);
			fail("write");
			continue;
		}
		mbedtls_sha256_update(&sha, chunk, n);
		written += n;
		if (written >= nextReport) {
			nextReport += ProgressStep;
			char buf[64];
			snprintf(buf, sizeof(buf), "{\"op\":\"progress\",\"recv\":%u}", unsigned(written));
			if (report) report(buf);
		}
	}
}

void post(Op op, size_t size = 0, const uint8_t* sha = nullptr) {
	Command cmd{ op, size, {} };
	if (sha) memcpy(cmd.sha, sha, sizeof(cmd.sha));
	xQueueSend(commands, &cmd, pdMS_TO_TICKS(100));
}

}  // namespace

namespace ota {

void init(Report r) {
	report = r;
	commands = xQueueCreate(4, sizeof(Command));
	stream = xStreamBufferCreate(BufferBytes, 1);
	xTaskCreate(task, "ota", 6144, nullptr, 4, nullptr);
}

void begin(size_t size, const uint8_t sha256[32]) {
	post(Op::Begin, size, sha256);
}

void data(const uint8_t* bytes, size_t len) {
	if (!running) return;
	size_t sent = 0;
	// Up to ~1 s of waiting: plenty for a sector erase, and the link layer
	// holds the next packets meanwhile.
	for (int tries = 0; sent < len && tries < 50; tries++) {
		sent += xStreamBufferSend(stream, bytes + sent, len - sent, pdMS_TO_TICKS(20));
	}
	received += sent;
	if (sent < len) ESP_LOGE(TAG, "dropped %u bytes", unsigned(len - sent));
}

void end() {
	post(Op::End);
}

void abort() {
	post(Op::Abort);
}

bool active() {
	return running;
}

bool failed() {
	return failure;
}

float progress() {
	const size_t total = expected;
	return total ? float(received) / float(total) : 0.0f;
}

}  // namespace ota
