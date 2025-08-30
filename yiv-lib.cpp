#include "yiv-lib.h"
#include <algorithm>
#include <random>
#include <stdexcept>
#include <cstring>

// stb_image for loading all formats
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// stb_image_write for saving
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace yiv {

// ==================== IMAGE ====================
bool Image::loadFromFile(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) return false;

    m_filePath = path;
    updatePixelData(data, width, height, channels);
    stbi_image_free(data);
    return true;
}

bool Image::loadPartial(const std::string& path, int x, int y, int w, int h) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) return false;
    if (x < 0 || y < 0 || x + w > width || y + h > height) {
        stbi_image_free(data);
        return false;
    }

    std::vector<unsigned char> partialPixels(w * h * channels);
    for (int row = 0; row < h; ++row) {
        std::memcpy(&partialPixels[row * w * channels],
                    &data[((y + row) * width + x) * channels],
                    w * channels);
    }
    updatePixelData(partialPixels.data(), w, h, channels);
    stbi_image_free(data);
    return true;
}

int Image::width() const { return m_width; }
int Image::height() const { return m_height; }
const unsigned char* Image::data() const { return m_pixels.data(); }
bool Image::hasAlpha() const { return m_channels == 4; }

void Image::updatePixelData(const unsigned char* data, int width, int height, int channels) {
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_pixels.assign(data, data + width * height * channels);
}

void Image::rotateClockwise() {
    std::vector<unsigned char> rotated(m_pixels.size());
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            for (int c = 0; c < m_channels; ++c) {
                rotated[(x * m_height + (m_height - y - 1)) * m_channels + c] =
                    m_pixels[(y * m_width + x) * m_channels + c];
            }
        }
    }
    std::swap(m_width, m_height);
    m_pixels = std::move(rotated);
}

void Image::rotateCounterClockwise() {
    rotateClockwise();
    rotateClockwise();
    rotateClockwise();
}

void Image::scale(float factor) {
    if (factor <= 0) return;
    int newW = int(m_width * factor);
    int newH = int(m_height * factor);
    std::vector<unsigned char> newPixels(newW * newH * m_channels);

    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            int srcX = int(x / factor);
            int srcY = int(y / factor);
            for (int c = 0; c < m_channels; ++c) {
                newPixels[(y * newW + x) * m_channels + c] =
                    m_pixels[(srcY * m_width + srcX) * m_channels + c];
            }
        }
    }
    m_pixels = std::move(newPixels);
    m_width = newW;
    m_height = newH;
}

// Filters (basic)
void Image::applyFilter(FilterType type) {
    switch(type) {
        case FilterType::Grayscale:
            for (int i = 0; i < m_width*m_height*m_channels; i+=m_channels) {
                unsigned char gray = static_cast<unsigned char>(
                    0.3*m_pixels[i] + 0.59*m_pixels[i+1] + 0.11*m_pixels[i+2]);
                m_pixels[i] = m_pixels[i+1] = m_pixels[i+2] = gray;
            }
            break;
        case FilterType::Invert:
            for (auto& px : m_pixels) px = 255 - px;
            break;
        case FilterType::Brightness:
            for (auto& px : m_pixels) px = std::min(255, px + 50);
            break;
        case FilterType::Contrast:
            for (auto& px : m_pixels) px = std::min(255, std::max(0, int((px-128)*1.2 + 128)));
            break;
    }
}

bool Image::saveAs(const std::string& path, ImageFormat format) {
    int success = 0;
    switch(format) {
        case ImageFormat::PNG:
            success = stbi_write_png(path.c_str(), m_width, m_height, m_channels, m_pixels.data(), m_width*m_channels);
            break;
        case ImageFormat::JPEG:
            success = stbi_write_jpg(path.c_str(), m_width, m_height, m_channels, m_pixels.data(), 90);
            break;
        case ImageFormat::BMP:
            success = stbi_write_bmp(path.c_str(), m_width, m_height, m_channels, m_pixels.data());
            break;
        case ImageFormat::TGA:
            success = stbi_write_tga(path.c_str(), m_width, m_height, m_channels, m_pixels.data());
            break;
        default:
            return false;
    }
    return success != 0;
}

std::shared_ptr<Image> Image::generateThumbnail(int maxWidth, int maxHeight) {
    float scaleFactor = std::min(float(maxWidth)/m_width, float(maxHeight)/m_height);
    auto thumb = std::make_shared<Image>();
    thumb->updatePixelData(m_pixels.data(), m_width, m_height, m_channels);
    thumb->scale(scaleFactor);
    return thumb;
}

std::string Image::getMetadata(const std::string& key) const {
    // TODO: Implement EXIF parsing if needed
    return "";
}

// ==================== IMAGELIST ====================
void ImageList::add(std::shared_ptr<Image> img) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_images.push_back(img);
}

void ImageList::remove(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_images.size()) return;
    m_images.erase(m_images.begin() + index);
}

std::shared_ptr<Image> ImageList::at(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_images.size()) return nullptr;
    return m_images[index];
}

size_t ImageList::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_images.size();
}

void ImageList::shuffle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_images.begin(), m_images.end(), g);
}

void ImageList::sort(bool (*comparator)(std::shared_ptr<Image>, std::shared_ptr<Image>)) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::sort(m_images.begin(), m_images.end(), comparator);
}

void ImageList::lock() { m_mutex.lock(); }
void ImageList::unlock() { m_mutex.unlock(); }

} // namespace yiv
