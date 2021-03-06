/*
 *

How to get info:

./rawer "/media/igor/Safari/SPIM_data/1x3 tile 100 section 2ch 50t/8.czi"

How to dump one 3D image:

./rawer "/media/igor/Safari/SPIM_data/1x3 tile 100 section 2ch 50t/8.czi" T60_C0_I0_V2

 *
 */

//#include "copy_roi.h"

#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>

using str = std::string;
template<typename T>
using vec = std::vector<T>;
using sz = size_t;
using i64 = int64_t;
using i32 = int32_t;

#pragma pack( push, structs, 1)

struct SegmentHeader {
  /// sequence of up to 15 ascii characters, 0 terminated
  char Id[16];
  /// number of bytes allocated for this segment
  i64 AllocatedSize;
  /// currently used number of bytes
  i64 UsedSize;
};

struct FileHeaderSegment {
  i32 VersionMajor;
  i32 VersionMinor;
  i32 Reserved1;
  i32 Reserved2;
  /// unique GUID of master file (16 bytes)
  i64 PrimaryFileGuid[2];
  /// unique per file (16 bytes)
  i64 FileGuid[2];
  /// Part number in multifile scenarios
  i32 FilePart;
  /// File position of the SubBlockDirectory Segment
  i64 DirectoryPosition;
  /// File position of the Metadata Segment
  i64 MetaDataPosition;
  /**
  * bool, represented by 4 bytes (true 0xffff false 0). This flag
  * indicates a currently inconsistent situation (e.g. updating
  * Index, Directory or Metadata segment). Readers should either
  * wait until this flag is reset (in case that a writer is still
  * accessing the file), or try a recovery procedure by scanning
  * all segments.
  */
  i32 UpdatePending;

  /// File position of the AttachmentDirectory Segment
  i64 AttachmentDirectoryPosition;
};

struct DirectorySegment {
  /// The number of entries
  i32 EntryCount;
  char Reserved[124];
};

struct DirectoryEntry {
  /// "DV"
  char SchemaType[2];
  /// The type of the image pixels
  i32 PixelType;
  /// Seek offset of the referenced SubBlockSegment relative to the first byte of the file
  i64 FilePosition;
  /// Reserved
  i32 FilePart;
  /// The compression segment data
  i32 Compression;
  /**
  * [INTERNAL] Contains information for automatic image
  * pyramids using SubBlocks of different resolution, current
  * values are: None=0, SingleBlock=1, MultiBlock=2.
  */
  char PyramidType[1];
  char Reserved1[1];
  char Reserved2[4];
  /// Number of entries. Minimum is 1.
  i32 DimensionCount;
};

struct DirectoryEntryDimension {
  /// Typically 1 Byte ANSI e.g. 'X'
  char Dimension[4];
  /// Start position / index. May be < 0
  i32 Start;
  /// Size in units of pixels (logical size). Must be > 0
  i32 Size;
  /// Physical start coordinate (units e.g. micrometers or seconds)
  float StartCoordinate;
  /// Stored size (if sub / supersampling, else 0)
  i32 StoredSize;
};

struct SubBlockSegment {
  /// Size of the metadata section.
  i32 MetadataSize;
  /// Size of the optional attachment section.
  i32 AttachmentSize;
  /// Size of the data section.
  i64 DataSize;
};

#pragma pack( pop, structs)


template<typename T>
void read(T& data, FILE* in, int count = 1)
{
  fread(&data, sizeof(T), count, in);
}

void seek(i64 pos, FILE* f)
{
#ifdef GCC
  fseeko(f, pos, SEEK_SET);
#else
  _fseeki64(f, pos, SEEK_SET);
#endif
}

int fileopen(FILE** f, const char* path, const char* flags)
{
#ifdef GCC
  *f = fopen(path, flags);
  return *f == 0 ? 1 : 0;
#else
  return fopen_s(f, path, flags);
#endif
}

static bool endsWith (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}


int main(int argc, char* argv[])
{
  sz default_img_index = 1;
  str default_imgs[] = { "lsm", "tiles", "time" };
  str in_filename = argc > 1 ? str(argv[1]) : "../data/" + default_imgs[default_img_index];
  str out_filename = argc > 3 ? str(argv[3]) : "stdout";

  str suffix = argc > 2 ? argv[2] : "";

  bool dump = out_filename == "stdout";

  //auto in = fopen(in_filename.c_str(), "rb");
  FILE* in;
  if (fileopen(&in, in_filename.c_str(), "rb") != 0) {
    if (fileopen(&in, (in_filename + ".czi").c_str(), "rb") != 0) return 1;
  }

  //char buf[1024 * 1024];

  SegmentHeader sh;
  
  FileHeaderSegment fileHeader;
  read(sh, in);
  read(fileHeader, in);

  seek(fileHeader.DirectoryPosition, in);

  DirectorySegment directorySegment;
  read(sh, in);
  read(directorySegment, in);

  i32 dirCount = directorySegment.EntryCount;
  if (dirCount == 0) return 1;

  vec<DirectoryEntry> directoryEntries(dirCount);
  vec<vec<DirectoryEntryDimension>> directoryEntryDimensions(dirCount);
  for (i32 i = 0; i < dirCount; i++) {
    read(directoryEntries[i], in);
    i32 dimCount = directoryEntries[i].DimensionCount;
    if (dimCount > 0) {
      directoryEntryDimensions[i].resize(dimCount);
      read(directoryEntryDimensions[i][0], in, dimCount);
    }
  }

  i32 dimSize[128];
  std::fill_n(dimSize, 128, 1);
  for (i32 i = 0; i < dirCount; i++) {
    i32 dimCount = directoryEntries[i].DimensionCount;
    if (!dump) {
		std::cout << " - Directory " << i << std::endl;
	}
    for (i32 j = 0; j < dimCount; j++) {
      const auto& entryDim = directoryEntryDimensions[i][j];
      auto& size = dimSize[entryDim.Dimension[0]];
      if (entryDim.Start + entryDim.Size > size) {
        size = entryDim.Start + entryDim.Size;
      }
    if (!dump) {
	  std::cout << "   - Dim " << entryDim.Dimension << " [" << entryDim.Start << ", " << entryDim.Size << "]" << std::endl;
  }
    }
  }

  dimSize['M'] = 1; // tiles!

  i32 numFiles = 1;
  i32 dimWeigth[128];
  i32 dimIndex[128];
  std::fill_n(dimIndex, 128, -1);
  for (i32 i = 0; i < 'X'; i++) {
    dimWeigth[i] = numFiles;
    numFiles *= dimSize[i];
  }
  for (i32 i = 'X'; i <= 'Z'; i++) {
    dimWeigth[i] = 0;
    dimIndex[i] = i - 'X';
  }
  for (i32 i = 'Z' + 1; i < 128; i++) {
    dimWeigth[i] = numFiles;
    numFiles *= dimSize[i];
  }

  dimWeigth['M'] = 0; // tiles!

  struct Dir {
    i32 index = 0;
    i32 startXYZ[3] = { 0, 0, 0 };
    i32 sizeXYZ[3] = { 1, 1, 1 };
  };
  vec<vec<Dir>> dirsPerFile(numFiles);
  for (i32 i = 0; i < dirCount; i++) {
    i32 dimCount = directoryEntries[i].DimensionCount;
    i32 fileIndex = 0; // accumulate...
    Dir dir;
    dir.index = i;
    for (i32 j = 0; j < dimCount; j++) {
      const auto& entryDim = directoryEntryDimensions[i][j];
      char d = entryDim.Dimension[0];
      fileIndex += dimWeigth[d] * entryDim.Start;
      if (dimIndex[d] >= 0) {
        dir.startXYZ[dimIndex[d]] = entryDim.Start;
        dir.sizeXYZ[dimIndex[d]] = entryDim.Size;
      }
    }
    dirsPerFile[fileIndex].push_back(dir);
  }

  i32 pixelSize = directoryEntries[0].PixelType == 1 ? 2 : directoryEntries[0].PixelType == 2 ? 4 : 1;

  const i32* sizeXYZ = dimSize + 'X';

if (!dump) {
  std::cout << " - Images " << numFiles << " [" << sizeXYZ[0] << ", " << sizeXYZ[1] << ", " << sizeXYZ[2] << "] " << pixelSize * 8 << "bps" << std::endl;
}
// * sizeXYZ[2]
  i64 bufSize = (i64)sizeXYZ[0] * sizeXYZ[1] * pixelSize;
  
  if (argc < 3) {
	bufSize = 0;
  }
    
  vec<char> block(bufSize);
  vec<char> image(bufSize);

  //v3 imageSize3{ sizeXYZ[0] * pixelSize, sizeXYZ[1], sizeXYZ[2] };
  //v3 zero{ 0, 0, 0 };

  SubBlockSegment subBlockSegment;
  for (i32 f = 0; f < numFiles; f++) {
    auto& dirs = dirsPerFile[f];
    if (dirs.empty()) continue;
    std::sort(dirs.begin(), dirs.end(), [](const Dir& a, const Dir& b){ return a.startXYZ[2] < b.startXYZ[2]; }); // sort by z and write slices!

    std::ostringstream fn;
    fn << out_filename;
    i32 ref_i = dirs.front().index;
    const auto& ref_dir = directoryEntries[ref_i];
    i32 dimCount = ref_dir.DimensionCount;
    for (i32 j = 0; j < dimCount; j++) {
      const auto& entryDim = directoryEntryDimensions[ref_i][j];
      if (dimWeigth[entryDim.Dimension[0]] > 0 && dimSize[entryDim.Dimension[0]] > 1) {
        fn << "_" << entryDim.Dimension << entryDim.Start;
      }
    }
    
    auto name = fn.str();
    if (!suffix.empty() && !endsWith(name, suffix)) {
		continue;
	}
    
    if (bufSize == 0) {
		std::cout << "Image " << f << ": " << name << std::endl;
		continue;
	}

    //std::ofstream meta(name + "_meta.txt", std::ios::trunc | std::ios::binary);
    //meta << "size XYZ: [" << sizeXYZ[0] << ", " << sizeXYZ[1] << ", " << sizeXYZ[2] << "]" << std::endl;

  if (true) {
    FILE* out;
    if (!dump && fileopen(&out, (name + "_data.raw").c_str(), "wb") != 0) {
      continue;
    }
    for (i32 i = 0; i < dirs.size(); i++) {
      const auto& dir = directoryEntries[dirs[i].index];
      seek(dir.FilePosition, in);
      read(sh, in);
      read(subBlockSegment, in);
      seek(dir.FilePosition + i64(sizeof(sh) + 256) + subBlockSegment.MetadataSize, in);
      
      sz size = (sz)subBlockSegment.DataSize;
      if (image.size() < size) {
        image.resize(size);
      }
      read(image[0], in, (i32)size);
      
    if (!dump) {
      fwrite(image.data(), 1, bufSize, out);
    }
    else {
	  //std::cout 
	  fwrite(image.data(), 1, bufSize, stdout);
	}
	
    }
    if (!dump) {
      fclose(out);
    }
      continue;
  }

    //std::fill(image.begin(), image.end(), 0);    
/*
    for (i32 i = 0; i < dirs.size(); i++) {
      const auto& dir = directoryEntries[dirs[i].index];
      seek(dir.FilePosition, in);
      read(sh, in);
      read(subBlockSegment, in);
      seek(dir.FilePosition + i64(sizeof(sh) + 256) + subBlockSegment.MetadataSize, in);

      sz size = (sz)subBlockSegment.DataSize;
      if (block.size() < size) {
        block.resize(size);
      }
      read(block[0], in, (i32)size);
      const auto& rs = dirs[i].startXYZ;
      const auto& bs = dirs[i].sizeXYZ;
      v3 blockSize3{ bs[0] * pixelSize, bs[1], bs[2] };
      copy_roi(block.data(), image.data(), blockSize3, imageSize3, zero, v3{ rs[0] * pixelSize, rs[1], rs[2] }, blockSize3);
    }
*/
    FILE* out;
    if (fileopen(&out, (name + "_data.raw").c_str(), "wb") != 0) {
      continue;
    }
    else {
      fwrite(image.data(), 1, bufSize, out);
      fclose(out);
    }
  }

if (bufSize == 0) {
  std::cout << " - Images " << numFiles << " [" << sizeXYZ[0] << ", " << sizeXYZ[1] << ", " << sizeXYZ[2] << "] " << pixelSize * 8 << "bps" << std::endl;
    for (i32 j = 0; j < 128; j++) {
      if (dimWeigth[j] > 0 && dimSize[j] > 1) {
        std::cout << (char)j << ": " << dimSize[j] << std::endl;
      }
    }
}
  fclose(in);
}
