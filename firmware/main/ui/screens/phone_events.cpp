// What the watch does when the phone needs attention: wake, sound, open a screen.
#include "screens.h"

#include "drivers/buzzer.h"

#include <esp_timer.h>

namespace {

class Events : public phone::Listener {
public:
	void notificationArrived(const phone::Notification& n) override {
		ui::wake();
		ui::devices().buzzer.chime();  // silent when sounds are off
		if (callScreenOpen() || pairingScreenOpen()) return;
		if (!showInPopup(n.uid)) ui::push(makeNotificationDetail(n.uid, true));
	}

	void callRinging(const phone::Notification& n) override {
		ui::wake();
		if (!callScreenOpen()) ui::push(makeCall(n.uid));
	}

	void pairingRequested(uint32_t code, bool confirm) override {
		ui::wake();
		ui::push(makePairing(code, confirm));
	}

	void pairingFinished(phone::PairResult result) override {
		if (::pairingFinished(result)) return;
		// Unasked: the phone tries old keys the watch no longer has. It retries
		// on every reconnect, so say it at most every ten minutes.
		if (result != phone::PairResult::KeyMissing) return;
		const int64_t now = esp_timer_get_time();
		if (lastKeyHint && now - lastKeyHint < 600LL * 1000000) return;
		lastKeyHint = now;
		ui::wake();
		ui::push(makePairingResult(result));
	}

private:
	int64_t lastKeyHint = 0;
};

Events events;

}  // namespace

void installPhoneEvents() {
	phone::setListener(&events);
}
