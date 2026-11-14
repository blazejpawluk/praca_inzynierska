/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja endpointu /capture (opis w capture.h)
*/

#include "camera.h"
#include "capture.h"

esp_err_t captureHandler(httpd_req_t* req) {
  // pierwsza klatka może być stara (zrobiona przed żądaniem), więc jest odrzucana
  camera_fb_t* fb = cameraGetFrame();
  if (fb) cameraReturnFrame(fb);
  fb = cameraGetFrame();

  cameraSetCommonHeaders(req);

  if (!fb) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "brak klatki", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=autko.jpg");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  cameraReturnFrame(fb);
  return res;
}
