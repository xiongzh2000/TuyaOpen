/**
 * @file cat_faces.c
 * @brief Emotion name to cat face image mapping
 */

#include <string.h>
#include "cat_faces.h"

typedef struct {
    const char         *name;
    const lv_img_dsc_t *img;
} CAT_FACE_MAP_T;

static const CAT_FACE_MAP_T sg_face_map[] = {
    {"NEUTRAL",      &icon_cat_neutral},
    {"HAPPY",        &icon_cat_happy},
    {"LAUGHING",     &icon_cat_happy},
    {"FUNNY",        &icon_cat_happy},
    {"SAD",          &icon_cat_sad},
    {"DISAPPOINTED", &icon_cat_sad},
    {"ANGRY",        &icon_cat_angry},
    {"ANNOYED",      &icon_cat_angry},
    {"SURPRISE",     &icon_cat_surprise},
    {"SHOCKED",      &icon_cat_surprise},
    {"SLEEP",        &icon_cat_sleep},
    {"RELAXED",      &icon_cat_sleep},
    {"WINK",         &icon_cat_happy},
    {"LOVING",       &icon_cat_happy},
    {"FEARFUL",      &icon_cat_surprise},
    {"THINKING",     &icon_cat_neutral},
    {"CONFUSED",     &icon_cat_surprise},
    {"COOL",         &icon_cat_happy},
    {"WAKEUP",       &icon_cat_surprise},
    {"TOUCH",        &icon_cat_happy},
};

const lv_img_dsc_t *cat_face_get_by_emotion(const char *emotion_name)
{
    if (emotion_name == NULL) {
        return &icon_cat_neutral;
    }

    for (int i = 0; i < (int)(sizeof(sg_face_map) / sizeof(sg_face_map[0])); i++) {
        if (strcmp(emotion_name, sg_face_map[i].name) == 0) {
            return sg_face_map[i].img;
        }
    }

    return &icon_cat_neutral;
}
