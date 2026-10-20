import Foundation

/// Turns an iOS time zone into the POSIX TZ string the watch's C library
/// understands, e.g. Europe/Kyiv -> "<+02>-2<+03>,M3.5.0/3,M10.5.0/4".
/// DST rules come from the zone's next two transitions.
enum PosixTimeZone {
    static func string(for zone: TimeZone, now: Date = Date()) -> String {
        guard let first = zone.nextDaylightSavingTimeTransition(after: now),
            let second = zone.nextDaylightSavingTimeTransition(after: first)
        else {
            return name(zone.secondsFromGMT(for: now)) + offset(zone.secondsFromGMT(for: now))
        }
        // Offsets on each side of the first transition tell which one starts DST.
        let before = zone.secondsFromGMT(for: first.addingTimeInterval(-1))
        let after = zone.secondsFromGMT(for: first)
        let (standard, daylight) = after > before ? (before, after) : (after, before)
        let (start, end) = after > before ? (first, second) : (second, first)
        // POSIX rule times are local wall time just before the switch: the
        // start in standard time, the end in daylight time.
        return name(standard) + offset(standard) + name(daylight) + "," + rule(start, offsetBefore: standard) + ","
            + rule(end, offsetBefore: daylight)
    }

    /// Quoted numeric name, e.g. "<+03>" or "<+0530>".
    private static func name(_ seconds: Int) -> String {
        let sign = seconds < 0 ? "-" : "+"
        let h = abs(seconds) / 3600, m = abs(seconds) % 3600 / 60
        return m == 0 ? String(format: "<%@%02d>", sign, h) : String(format: "<%@%02d%02d>", sign, h, m)
    }

    /// POSIX offsets count west as positive: UTC+2 is "-2".
    private static func offset(_ seconds: Int) -> String {
        let sign = seconds > 0 ? "-" : ""
        let h = abs(seconds) / 3600, m = abs(seconds) % 3600 / 60
        return m == 0 ? "\(sign)\(h)" : "\(sign)\(h):" + String(format: "%02d", m)
    }

    /// "Mm.w.d/time": month, week of month (5 = last), weekday (0 = Sunday).
    private static func rule(_ transition: Date, offsetBefore: Int) -> String {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = TimeZone(secondsFromGMT: offsetBefore)!
        let c = calendar.dateComponents([.month, .day, .weekday, .hour, .minute, .second], from: transition)
        let day = c.day!
        let daysInMonth = calendar.range(of: .day, in: .month, for: transition)!.count
        let week = day + 7 > daysInMonth ? 5 : (day - 1) / 7 + 1
        var time = "\(c.hour!)"
        if c.minute! != 0 || c.second! != 0 { time += String(format: ":%02d", c.minute!) }
        if c.second! != 0 { time += String(format: ":%02d", c.second!) }
        return "M\(c.month!).\(week).\(c.weekday! - 1)/\(time)"
    }
}
