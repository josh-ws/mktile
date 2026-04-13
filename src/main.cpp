#include "raylib.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct CImage {
    Image image;
    CImage(const std::string &s)
    {
        image = LoadImage(s.c_str());
        if (!image.data)
            throw std::invalid_argument("unable to read file: " + s);
    }
    CImage(int width, int height, Color color)
    {
        image = GenImageColor(width, height, color);
    }
    ~CImage()
    {
        UnloadImage(image);
    }
    CImage(const CImage &) = delete;
    CImage &operator=(const CImage &) = delete;
};

struct Tile {
    std::vector<Color> pixels;
    Tile(const Image &i, int x, int y, int w, int h)
    {
        const auto *src = static_cast<const Color *>(i.data);
        const auto  size = w * h;
        pixels.resize(size);
        for (auto row = 0; row < h; row++)
            for (auto col = 0; col < w; col++)
                pixels[row * w + col] = src[(y + row) * i.width + (x + col)];
    }
    bool operator==(const Tile &other) const
    {
        return pixels.size() == other.pixels.size() && std::memcmp(pixels.data(), other.pixels.data(), pixels.size() * sizeof(Color)) == 0;
    }
};

template <>
struct std::hash<Tile> {
    static constexpr std::size_t basis = 14695981039346656037ULL;
    static constexpr std::size_t prime = 1099511628211ULL;

    std::size_t operator()(const Tile &t) const noexcept
    {
        const auto *data = reinterpret_cast<const unsigned char *>(t.pixels.data());
        const auto  len = t.pixels.size() * sizeof(Color);
        std::size_t h = basis;
        for (std::size_t i = 0; i < len; i++) {
            h ^= data[i];
            h *= prime;
        }
        return h;
    }
};

constexpr int ExpectedArguments = 5;

struct Args {
    std::string input, output;
    int         width;
    int         height;
    Args(const std::vector<std::string> &args)
    {
        assert(args.size() == ExpectedArguments);
        input = args[1];
        try {
            width = std::stoi(args[2]);
        } catch (const std::exception &) {
            throw std::runtime_error("invalid width");
        }
        try {
            height = std::stoi(args[3]);
        } catch (const std::exception &) {
            throw std::runtime_error("invalid height");
        }
        output = args[4];
    }
};

[[noreturn]] void ExitWithError(std::string_view progName, std::string_view detail)
{
    std::cerr << detail << "\n"
              << "Usage: " << progName << " <infile> <width> <height> <outfile>\n"
              << "  infile: Source image to create a tilemap from\n"
              << "  width: Width of each tile\n"
              << "  height: Height of each tile\n"
              << "  outfile: Target to write the result to\n";
    std::exit(EXIT_FAILURE);
}

int main(int argc, const char **argv)
{
    const auto sArgs = std::vector<std::string>(argv, argv + argc);
    if (sArgs.size() != ExpectedArguments) {
        ExitWithError(argv[0], "wrong number of arguments");
    }

    try {
        const auto args = Args(sArgs);
        if (args.width <= 0 || args.height <= 0) {
            ExitWithError(argv[0], "invalid width and/or height, both must be integers >0");
        }

        auto src = CImage(std::string(args.input));
        ImageFormat(&src.image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        if (src.image.width % args.width != 0 || src.image.height % args.height != 0) {
            std::cerr << "WARN: image size is not evenly divisible by tile size, remaining image will be truncated" << "\n";
        }

        const auto rows = src.image.height / args.height;
        const auto columns = src.image.width / args.width;
        auto       set = std::unordered_set<Tile>();
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < columns; c++) {
                const auto tile = Tile(src.image, c * args.width, r * args.height, args.width, args.height);
                set.insert(tile);
            }
        }

        const auto tot = set.size();
        const auto outColumns = static_cast<size_t>(std::ceil(std::sqrt(tot)));
        const auto outRows = (tot + outColumns - 1) / outColumns;

        auto   output = CImage(args.width * static_cast<int>(outColumns), args.height * outRows, BLANK);
        auto   dest = static_cast<Color *>(output.image.data);
        size_t i = 0;
        for (const auto &tile : set) {
            auto gc = i % outColumns;
            auto gr = i / outColumns;
            for (auto row = 0; row < args.height; row++)
                for (auto col = 0; col < args.width; col++) {
                    const auto index = (gr * args.height + row) * output.image.width + (gc * args.width + col);
                    dest[index] = tile.pixels[row * args.width + col];
                }
            i += 1;
        }

        ExportImage(output.image, args.output.c_str());
        return EXIT_SUCCESS;
    } catch (const std::exception &ex) {
        ExitWithError(argv[0], ex.what());
    }
}
