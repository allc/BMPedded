#include <stdlib.h>
#include "bmp.h"
#include "ili934x.h"

inline size_t calc_row_size(bmp_info_header * dibHeader) {
  /* Weird formula because BMP row sizes are padded up to a multiple of 4 bytes. */
  return (((dibHeader->bitsPerPixel * dibHeader->imageWidth) + 31) / 32) * 4;
}

/* Load in the headers and data necessary to start retrieving image data. */
/* Grossly violates strict aliasing, so compile without it. */
status_t init_bmp(bmp_image_loader_state * loaderState, bmp_need_more_bytes dataRetrievalFunc)
{
  bmp_data_request data_request;

  loaderState->data_request_func = dataRetrievalFunc;

  data_request.buffer = (void *)&loaderState->fileHeader;
  data_request.bufferSize = sizeof(bmp_file_header);
  data_request.dataOffset = 0;

  dataRetrievalFunc(&data_request);

  /* For now, only support BMPINFOHEADER, as CORE doesn't have 16 bit color. */
  data_request.buffer = (void *)&loaderState->dibHeader;
  data_request.bufferSize = sizeof(bmp_info_header);
  data_request.dataOffset = sizeof(bmp_file_header);
  dataRetrievalFunc(&data_request);

  loaderState->rowSize = calc_row_size(&loaderState->dibHeader);
  loaderState->imageDataRow = (uint16_t *)malloc(loaderState->rowSize);
  loaderState->currentRow = 0;
  return STATUS_OK;
}

status_t bmp_next_row(bmp_image_loader_state * loaderState) 
{
  bmp_data_request data_request;

  data_request.buffer = (void*)loaderState->imageDataRow;
  data_request.bufferSize = loaderState->rowSize;
  data_request.dataOffset = loaderState->fileHeader.imageDataOffset + loaderState->rowSize * loaderState->currentRow;

  loaderState->data_request_func(&data_request);
  loaderState->currentRow += 1;
  return STATUS_OK;
}

inline void draw_row_bmp(uint16_t x, uint16_t y, uint16_t width, uint16_t * data)
{
    write_cmd(COLUMN_ADDRESS_SET);
    write_data16(x);
    write_data16(x+width);
    write_cmd(PAGE_ADDRESS_SET);
    write_data16(y);
    write_data16(y);
    write_cmd(MEMORY_WRITE);
    uint16_t rowPos;
    for(rowPos = width; rowPos > 0; rowPos--) 
    {
      write_data16(data[rowPos-1]);
    }
}

status_t display_segment_bmp(uint16_t x, uint16_t y, rectangle * area, bmp_image_loader_state * loaderState)
{
  loaderState->currentRow = loaderState->dibHeader.imageHeight - area->bottom;
  uint16_t rowWidth = (area->right - area->left);
  uint16_t currentY = y + (area->bottom - area->top);
  while(currentY > y)
  {
    bmp_next_row(loaderState);
    draw_row_bmp(x, currentY, rowWidth, (void*)(loaderState->imageDataRow+area->left));
    currentY--;
  }
}
