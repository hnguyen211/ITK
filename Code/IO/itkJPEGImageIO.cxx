/*=========================================================================


  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkJPEGImageIO.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifdef _MSC_VER
// warning C4611: interaction between '_setjmp' and C++ object 
// destruction is non-portable
#pragma warning( disable : 4611 )
#endif 

#include "itkJPEGImageIO.h"
#include "itkRGBPixel.h"
#include "itkRGBAPixel.h"
#include <stdio.h>

extern "C" {
// The regular jpeg lossy lib is the 8bits one:
#include <itkjpeg/8/jpeglib.h>
#include <setjmp.h>
}


// create an error handler for jpeg that
// can longjmp out of the jpeg library 
struct itk_jpeg_error_mgr 
{
  struct jpeg_error_mgr pub;    /* "public" fields */
  jmp_buf setjmp_buffer;        /* for return to caller */
};


namespace itk
{

// simple class to call fopen on construct and
// fclose on destruct
class JPEGFileWrapper
{
  public:
  JPEGFileWrapper(const char * const fname, const char * const openMode):m_FilePointer(NULL)
  {
    m_FilePointer = fopen(fname, openMode);
  }
  virtual ~JPEGFileWrapper()
  {
    if(m_FilePointer!=NULL)
      {
      fclose(m_FilePointer);
      }
  }
  FILE* m_FilePointer;
};

bool JPEGImageIO::CanReadFile(const char* file) 
{ 
  // First check the extension
  std::string filename = file;
  if(  filename == "" )
    {
    itkDebugMacro(<<"No filename specified.");
    return false;
    }

  bool extensionFound = false;
  std::string::size_type JPEGPos = filename.rfind(".jpeg");
  if ((JPEGPos != std::string::npos)
      && (JPEGPos == filename.length() - 5))
    {
    extensionFound = true;
    }

  JPEGPos = filename.rfind(".JPEG");
  if ((JPEGPos != std::string::npos)
      && (JPEGPos == filename.length() - 5))
    {
    extensionFound = true;
    }

  JPEGPos = filename.rfind(".jpg");
  if ((JPEGPos != std::string::npos)
      && (JPEGPos == filename.length() - 4))
    {
    extensionFound = true;
    }

  JPEGPos = filename.rfind(".JPG");
  if ((JPEGPos != std::string::npos)
      && (JPEGPos == filename.length() - 4))
    {
    extensionFound = true;
    }

  if( !extensionFound )
    {
    itkDebugMacro(<<"The filename extension is not recognized");
    return false;
    }

  // Now check the file header
  JPEGFileWrapper JPEGfp(file,"rb");
  if(JPEGfp.m_FilePointer==NULL)
    {
    return false;
    }

  // read the first two bytes
  char magic[2];
  int n = static_cast<int>(fread(magic, sizeof(magic), 1, JPEGfp.m_FilePointer));
  if (n != 1) 
    {
    return false;
    }
  
  // check for the magic stuff:
  // 0xFF followed by 0xD8
  if( ( (static_cast<unsigned char>(magic[0]) != 0xFF) || 
        (static_cast<unsigned char>(magic[1]) != 0xD8) ) )
    {
    return false;
    }
  // go back to the start of the file
  fseek(JPEGfp.m_FilePointer, 0, SEEK_SET);
  // magic number is ok, try and read the header
  struct itk_jpeg_error_mgr jerr;
  struct jpeg_decompress_struct cinfo;
  cinfo.err = jpeg_std_error(&jerr.pub);
  // set the jump point, if there is a jpeg error or warning
  // this will evaluate to true
  if (setjmp(jerr.setjmp_buffer))
    {
    // clean up
    jpeg_destroy_decompress(&cinfo);
    // this is not a valid jpeg file
    return false;
    }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);
  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, JPEGfp.m_FilePointer);
  /* Step 3: read file parameters with jpeg_read_header() */
  jpeg_read_header(&cinfo, TRUE);
  
  // if no errors have occurred yet, then it must be jpeg
  jpeg_destroy_decompress(&cinfo);

  return true;
}
  

  
void JPEGImageIO::ReadVolume(void*)
{
  
}

  
void JPEGImageIO::Read(void* buffer)
{
  unsigned int ui;

  // use this class so return will call close
  JPEGFileWrapper JPEGfp(this->GetFileName(),"rb"); 
  FILE* fp = JPEGfp.m_FilePointer;
  if(!fp)
    {
    itkExceptionMacro("Error JPEGImageIO could not open file: " 
                      << this->GetFileName());
    return;
    }
 
  // create jpeg decompression object and error handler
  struct jpeg_decompress_struct cinfo;
  struct itk_jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr.pub);
  if (setjmp(jerr.setjmp_buffer))
    {
    // clean up
    jpeg_destroy_decompress(&cinfo);
    itkExceptionMacro("libjpeg could not read file: "
                            << this->GetFileName());
    // this is not a valid jpeg file
    return;
    }
  
  jpeg_create_decompress(&cinfo);

  // set the source file
  jpeg_stdio_src(&cinfo, fp);

  // read the header
  jpeg_read_header(&cinfo, TRUE);

  // prepare to read the bulk data
  jpeg_start_decompress(&cinfo);

  unsigned long rowbytes = cinfo.output_components * cinfo.output_width;
  unsigned char *tempImage = static_cast<unsigned char*>(buffer);


  JSAMPROW *row_pointers = new JSAMPROW [cinfo.output_height];
  for (ui = 0; ui < cinfo.output_height; ++ui)
    {
    row_pointers[ ui  ] = tempImage + rowbytes*ui;
    }

  // read the bulk data
  unsigned int remainingRows;
  while (cinfo.output_scanline < cinfo.output_height)
    {
    remainingRows = cinfo.output_height - cinfo.output_scanline;
    jpeg_read_scanlines(&cinfo, &row_pointers[cinfo.output_scanline],
                        remainingRows);
    }


  // finish the decompression step
  jpeg_finish_decompress(&cinfo);

  // destroy the decompression object
  jpeg_destroy_decompress(&cinfo);

  delete [] row_pointers;
}


JPEGImageIO::JPEGImageIO()
{
  this->SetNumberOfDimensions(2);
  m_PixelType = SCALAR;
  m_ComponentType = UCHAR;
  m_UseCompression = false;
  m_Quality = 95;
  m_Progressive = true;
  m_Spacing[0] = 1.0;
  m_Spacing[1] = 1.0;
  
  m_Origin[0] = 0.0;
  m_Origin[1] = 0.0;
}

JPEGImageIO::~JPEGImageIO()
{
}

void JPEGImageIO::PrintSelf(std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
  os << indent << "Quality : " << m_Quality << "\n";
  os << indent << "Progressive : " << m_Progressive << "\n";
}

  
  
void JPEGImageIO::ReadImageInformation()
{
  m_Spacing[0] = 1.0;  // We'll look for JPEG pixel size information later,
  m_Spacing[1] = 1.0;  // but set the defaults now

  m_Origin[0] = 0.0;
  m_Origin[1] = 0.0;

  // use this class so return will call close
  JPEGFileWrapper JPEGfp(m_FileName.c_str(),"rb");
  FILE* fp = JPEGfp.m_FilePointer;
  if(!fp)
    {
    itkExceptionMacro("Error JPEGImageIO could not open file: " 
                      << this->GetFileName());
    return;
    }

  // create jpeg decompression object and error handler
  struct jpeg_decompress_struct cinfo;
  struct itk_jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr.pub); 
  if (setjmp(jerr.setjmp_buffer))
    {
    // clean up
    jpeg_destroy_decompress(&cinfo);
    // this is not a valid jpeg file 
    itkExceptionMacro("Error JPEGImageIO could not open file: " 
                      << this->GetFileName());
    return;
    }
  jpeg_create_decompress(&cinfo);

  // set the source file
  jpeg_stdio_src(&cinfo, fp);

  // read the header
  jpeg_read_header(&cinfo, TRUE);

  // force the output image size to be calculated (we could have used
  // cinfo.image_height etc. but that would preclude using libjpeg's
  // ability to scale an image on input).
  jpeg_calc_output_dimensions(&cinfo);

  // pull out the width/height
  this->SetNumberOfDimensions(2);
  m_Dimensions[0] = cinfo.output_width;
  m_Dimensions[1] = cinfo.output_height;

  this->SetNumberOfComponents(cinfo.output_components);

  switch (this->GetNumberOfComponents())
    {
    case 1: m_PixelType = SCALAR;
      break;
    case 2: m_PixelType = VECTOR;
      break;
    case 3: m_PixelType = RGB;
      break;
    case 4: m_PixelType = RGBA;
      break;
    }
      

  // close the file
  jpeg_destroy_decompress(&cinfo);

  return;
}

bool JPEGImageIO::CanWriteFile( const char * name )
{
  std::string filename = name;

  if (filename == "")
    {
    return false;
    }
  
  std::string::size_type JPEGPos = filename.rfind(".jpeg");
  if ( (JPEGPos != std::string::npos)
       && (JPEGPos == filename.length() - 5) )
    {
    return true;
    }

  JPEGPos = filename.rfind(".JPEG");
  if ( (JPEGPos != std::string::npos)
       && (JPEGPos == filename.length() - 5) )
    {
    return true;
    }

  JPEGPos = filename.rfind(".jpg");
  if ( (JPEGPos != std::string::npos)
       && (JPEGPos == filename.length() - 4) )
    {
    return true;
    }


  JPEGPos = filename.rfind(".JPG");
  if ( (JPEGPos != std::string::npos)
       && (JPEGPos == filename.length() - 4) )
    {
    return true;
    }


  return false;
}


void JPEGImageIO::WriteImageInformation(void)
{
}

void JPEGImageIO::Write(const void* buffer)
{
  ImageIORegion ioRegion = this->GetIORegion();

  // Make sure the region to be written is 2D
  if ( ioRegion.GetRegionDimension() != 2 )
    {
    itkExceptionMacro(<<"JPEG Writer can only write 2-dimensional images");
    }

  if(this->GetComponentType() != UCHAR)
    {
    itkExceptionMacro(<<"JPEG supports unsigned char only");
    }
  
  this->WriteSlice(m_FileName, buffer);
}

void JPEGImageIO::WriteSlice(std::string& fileName, const void* buffer)
{
  const unsigned char *outPtr = ( (const unsigned char*) buffer);

  // use this class so return will call close
  JPEGFileWrapper JPEGfp(fileName.c_str(),"wb");
  FILE* fp = JPEGfp.m_FilePointer;
  if(!fp)
    {
    itkExceptionMacro("Unable to open file " << fileName);
    }

  // Call the correct templated function for the output
  unsigned int ui;

  // overriding jpeg_error_mgr so we don't exit when an error happens
  // Create the jpeg compression object and error handler
  //struct jpeg_compress_struct cinfo;
  //struct itk_jpeg_error_mgr jerr;
  
  struct itk_jpeg_error_mgr jerr;
  struct jpeg_compress_struct cinfo;
  cinfo.err = jpeg_std_error(&jerr.pub);
  // set the jump point, if there is a jpeg error or warning
  // this will evaluate to true
  if (setjmp(jerr.setjmp_buffer))
    {
    jpeg_destroy_compress(&cinfo);
    itkExceptionMacro(<<"JPEG : Out of disk space");
    return;
    }
  
  jpeg_create_compress(&cinfo);

  // set the destination file
  //struct jpeg_destination_mgr compressionDestination;
  jpeg_stdio_dest(&cinfo, fp);
  
  // set the information about image
  unsigned int width, height;
  width =  m_Dimensions[0];
  height = m_Dimensions[1];  

  cinfo.image_width = width;
  cinfo.image_height = height;

  cinfo.input_components = this->GetNumberOfComponents();
  unsigned int numComp = this->GetNumberOfComponents();

  switch (cinfo.input_components)
    {
    case 1: cinfo.in_color_space = JCS_GRAYSCALE;
      break;
    case 3: cinfo.in_color_space = JCS_RGB;
      break;
    default: cinfo.in_color_space = JCS_UNKNOWN;
      break;
    }

  // set the compression parameters
  jpeg_set_defaults(&cinfo);         // start with reasonable defaults
  jpeg_set_quality(&cinfo, m_Quality, TRUE);
  if (m_Progressive)
    {
    jpeg_simple_progression(&cinfo);
    }
  
  // start compression
  jpeg_start_compress(&cinfo, TRUE);

  // write the data. in jpeg, the first row is the top row of the image
  JSAMPROW *row_pointers = new JSAMPROW [height];
  int rowInc = numComp*width;
  for (ui = 0; ui < height; ui++)
    {
    row_pointers[ui] = const_cast<JSAMPROW>(outPtr);
    outPtr = const_cast<unsigned char *>(outPtr) + rowInc;
    }
  jpeg_write_scanlines(&cinfo, row_pointers, height);
  
  if (fflush(fp) == EOF)
    {
    itkExceptionMacro(<<"JPEG : Out of disk space"); 
    return;
    }

  // finish the compression
  jpeg_finish_compress(&cinfo);
  
  // clean up and close the file
  delete [] row_pointers;
  jpeg_destroy_compress(&cinfo); 
}


} // end namespace itk

