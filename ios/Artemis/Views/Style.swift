import SwiftUI

/// The watch's palette (firmware ui/theme.h).
enum Palette {
    static let black = Color(red: 0, green: 0, blue: 0)
    static let white = Color(red: 0xF2 / 255, green: 0xF2 / 255, blue: 0xF2 / 255)
    static let red = Color(red: 0xFC / 255, green: 0x3D / 255, blue: 0x21 / 255)
    static let grey = Color(red: 0x80 / 255, green: 0x80 / 255, blue: 0x80 / 255)
}

/// Ordered (Bayer 8x8) dither of `tone` (0...1) in square "pixels" of `pixel`
/// points, like the watch draws its shades.
struct DitherFill: View {
    var tone: Double
    var color: Color = Palette.white
    var pixel: CGFloat = 2

    private static let bayer: [[Int]] = [
        [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
        [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
        [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
        [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21],
    ]

    var body: some View {
        Canvas { context, size in
            let threshold = Int(tone * 64)
            let cols = Int(size.width / pixel) + 1, rows = Int(size.height / pixel) + 1
            var path = Path()
            for y in 0..<rows {
                for x in 0..<cols where threshold > Self.bayer[y & 7][x & 7] {
                    path.addRect(CGRect(x: CGFloat(x) * pixel, y: CGFloat(y) * pixel, width: pixel, height: pixel))
                }
            }
            context.fill(path, with: .color(color))
        }
    }
}

/// Outlined bar with a checker fill and a red head, like the watch's music
/// and update screens.
struct DitherProgress: View {
    var value: Double

    var body: some View {
        GeometryReader { geo in
            let w = max(0, min(1, value)) * (geo.size.width - 4)
            ZStack(alignment: .leading) {
                Rectangle().stroke(Palette.white, lineWidth: 1)
                DitherFill(tone: 0.5).frame(width: w).padding(2)
                Rectangle().fill(Palette.red).frame(width: 4).offset(x: w)
            }
        }
        .frame(height: 14)
    }
}

extension View {
    func panel() -> some View {
        listRowBackground(Palette.black).listRowSeparatorTint(Palette.grey.opacity(0.4))
    }
}
