/**
 * @file cat_faces.h
 * @brief Cartoon cat face image declarations and emotion mapping
 */

#ifndef __CAT_FACES_H__
#define __CAT_FACES_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

LV_IMG_DECLARE(icon_cat_neutral);
LV_IMG_DECLARE(icon_cat_happy);
LV_IMG_DECLARE(icon_cat_sad);
LV_IMG_DECLARE(icon_cat_angry);
LV_IMG_DECLARE(icon_cat_surprise);
LV_IMG_DECLARE(icon_cat_sleep);

const lv_img_dsc_t *cat_face_get_by_emotion(const char *emotion_name);

#ifdef __cplusplus
}
#endif

#endif /* __CAT_FACES_H__ */
