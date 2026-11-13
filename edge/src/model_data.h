#pragma once

// File này sẽ được ghi đè tự động bởi `ml/export.py` sau khi huấn luyện & lượng tử hoá int8.
// Giá trị dưới đây là placeholder để firmware biên dịch được trước khi có model thật.

#define MODEL_VERSION "0.0.0-placeholder"

alignas(16) const unsigned char g_model_data[] = {0x00};
const unsigned int g_model_data_len = 0;
