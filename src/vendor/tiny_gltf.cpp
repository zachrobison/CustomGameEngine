#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

// Stubs — we only read geometry from GLB, not textures.
namespace tinygltf {
bool LoadImageData(Image*, const int, std::string*, std::string*,
                   int, int, const unsigned char*, int, void*) { return true; }
bool WriteImageData(const std::string*, const std::string*,
                    const Image*, bool, const URICallbacks*, std::string*, void*) { return true; }
}
