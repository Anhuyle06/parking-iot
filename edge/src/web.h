#pragma once

/**
 * @brief Khởi tạo Webserver (port 80) cho hiệu chỉnh ROI, thu ảnh và điều khiển local (T07).
 *
 * Các endpoint:
 *  - GET  /            : Trang HTML + Canvas vẽ 4 khung ô + vùng cấm
 *  - GET  /capture     : Trả ảnh JPEG toàn khung 320x240 cho công cụ ml/collect.py
 *  - GET  /roi         : Trả chuỗi roi_json hiện hành
 *  - GET  /api/status  : Trả JSON trạng thái 4 ô, vùng cấm, barrier, mode, thông số đo
 *  - POST /api/roi     : Lưu roi_json mới vào NVS và áp dụng ngay
 *  - POST /api/barrier : Mở/đóng barrier hoặc tắt còi tại chỗ
 */
void webInit();

/**
 * @brief Xử lý request HTTP định kỳ trong vòng lặp.
 */
void webLoop();
