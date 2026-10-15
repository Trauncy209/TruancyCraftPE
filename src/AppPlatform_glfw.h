#ifndef APPPLATFORM_GLFW_H__
#define APPPLATFORM_GLFW_H__

#include "AppPlatform.h"
#include "platform/log.h"
#include "client/renderer/gles.h"
#include "world/level/storage/FolderMethods.h"
#include <png.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static void png_funcReadFile(png_structp pngPtr, png_bytep data, png_size_t length) {
	((std::istream*)png_get_io_ptr(pngPtr))->read((char*)data, length);
}

class AppPlatform_glfw: public AppPlatform
{
public:
    AppPlatform_glfw()
    {
    }

	BinaryBlob readAssetFile(const std::string& filename) {
		FILE* fp = fopen(("data/" + filename).c_str(), "r");
		if (!fp)
			return BinaryBlob();

		int size = getRemainingFileSize(fp);

		BinaryBlob blob;
		blob.size = size;
		blob.data = new unsigned char[size];

		fread(blob.data, 1, size, fp);
		fclose(fp);

		return blob;
	}

    void saveScreenshot(const std::string& filename, int glWidth, int glHeight) {
        //@todo
    }

    __inline unsigned int rgbToBgr(unsigned int p) {
        return (p & 0xff00ff00) | ((p >> 16) & 0xff) | ((p << 16) & 0xff0000);
    }

    TextureData loadTexture(const std::string& filename_, bool textureFolder)
	{
		TextureData out;

		std::string filename = textureFolder? "data/images/" + filename_
											: filename_;
		std::ifstream source(filename.c_str(), std::ios::binary);

		if (source) {
			png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

			if (!pngPtr)
				return out;

			png_infop infoPtr = png_create_info_struct(pngPtr);

			if (!infoPtr) {
				png_destroy_read_struct(&pngPtr, NULL, NULL);
				return out;
			}

			// Hack to get around the broken libpng for windows
			png_set_read_fn(pngPtr,(void*)&source, png_funcReadFile);

			png_read_info(pngPtr, infoPtr);
			// Java's supplied entity/equipment PNGs may be indexed, grayscale or RGB.
			// Normalize losslessly to the RGBA layout required by the native uploader.
			const int colorType=png_get_color_type(pngPtr,infoPtr),bitDepth=png_get_bit_depth(pngPtr,infoPtr);
			if(bitDepth==16)png_set_strip_16(pngPtr);
			if(colorType==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(pngPtr);
			if(colorType==PNG_COLOR_TYPE_GRAY&&bitDepth<8)png_set_expand_gray_1_2_4_to_8(pngPtr);
			const bool transparency=png_get_valid(pngPtr,infoPtr,PNG_INFO_tRNS)!=0;
			if(transparency)png_set_tRNS_to_alpha(pngPtr);
			if(colorType==PNG_COLOR_TYPE_GRAY||colorType==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(pngPtr);
			if(!(colorType&PNG_COLOR_MASK_ALPHA)&&!transparency)png_set_add_alpha(pngPtr,255,PNG_FILLER_AFTER);
			png_read_update_info(pngPtr,infoPtr);

			// Set up the texdata properties
			out.w = png_get_image_width(pngPtr, infoPtr);
			out.h = png_get_image_height(pngPtr, infoPtr);

			png_bytep* rowPtrs = new png_bytep[out.h];
			out.data = new unsigned char[4 * out.w * out.h];
			out.memoryHandledExternally = false;

			int rowStrideBytes = 4 * out.w;
			for (int i = 0; i < out.h; i++) {
				rowPtrs[i] = (png_bytep)&out.data[i*rowStrideBytes];
			}
			png_read_image(pngPtr, rowPtrs);

			// Teardown and return
			png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
			delete[] (png_bytep)rowPtrs;
			source.close();

			return out;
		}
		else
		{
			LOGI("Couldn't find file: %s\n", filename.c_str());
			return out;
		}
    }

    std::string getDateString(int s) {
        std::stringstream ss;
		ss << s << " s (UTC)";
		return ss.str();
	}

	virtual int checkLicense() {
		static int _z = 0;//20;
		_z--;
		if (_z < 0) return 0;
		//if (_z < 0) return 107;
		return -2;
	}

	virtual int getScreenWidth() { return 854; };
	virtual int getScreenHeight() { return 480; };

	virtual float getPixelsPerMillimeter();

	virtual bool supportsTouchscreen() { return true; }
	virtual bool hasBuyButtonWhenInvalidLicense() { return false; }

	virtual void openURL(const std::string& url) {
#ifdef _WIN32
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
	}

private:
};

void* winGLLoader(const char* name);
void glPatchDesktopCompat();

#endif /*APPPLATFORM_GLFW_H__*/
