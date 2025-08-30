/*****************************************************************************
 * yiv-lib.h: Yaznbook Image Viewer Library (C++ library)
 *****************************************************************************
 * Author: Yazan
 * Description: High-quality image library for apps
 * Supports: PNG, JPEG, BMP, GIF, TIFF, WebP, HEIF, etc.
 *****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace yiv {

enum class FilterType { Grayscale, Invert, Brightness, Contrast };
enum class ImageFormat { PNG, JPEG, BMP, GIF, TIFF, WEBP, HEIF };

class Image {
public:
    Image() = default;
    ~Image() = default;

    bool loadFromFile(const std::string& path);
    int width() const;
    int height() const;
    const unsigned char* data() const;

    void rotateClockwise();
    void rotateCounterClockwise();
    void scale(float factor);

    // New features
    bool hasAlpha() const;
    std::string getMetadata(const std::string& key) const;
    void applyFilter(FilterType type);
    bool saveAs(const std::string& path, ImageFormat format);
    std::shared_ptr<Image> generateThumbnail(int maxWidth, int maxHeight);
    bool loadPartial(const std::string& path, int x, int y, int width, int height);

private:
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    std::vector<unsigned char> m_pixels;
    std::string m_filePath;

    void updatePixelData(const unsigned char* data, int width, int height, int channels);
};

class ImageList {
public:
    ImageList() = default;
    ~ImageList() = default;

    void add(std::shared_ptr<Image> img);
    void remove(size_t index);
    std::shared_ptr<Image> at(size_t index);
    size_t count() const;

    void shuffle();
    void sort(bool (*comparator)(std::shared_ptr<Image>, std::shared_ptr<Image>));

    void lock();
    void unlock();

private:
    std::vector<std::shared_ptr<Image>> m_images;
    mutable std::mutex m_mutex; // thread safety
};

} // namespace yiv
