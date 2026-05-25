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
    /* 13 primary emotions */
    {"HAPPY",        &icon_cat_happy},        /* 开心 😆 */
    {"SAD",          &icon_cat_sad},           /* 悲伤 😥 */
    {"ANGRY",        &icon_cat_angry},         /* 生气 😤 */
    {"SURPRISE",     &icon_cat_surprise},      /* 惊讶 😲 */
    {"FEAR",         &icon_cat_fear},          /* 恐惧 😱 */
    {"DISGUST",      &icon_cat_disgust},       /* 厌恶 🙄 */
    {"EXCITED",      &icon_cat_excited},       /* 激动 🤩 */
    {"INDIFFERENT",  &icon_cat_indifferent},   /* 冷漠 😒 */
    {"NEUTRAL",      &icon_cat_neutral},       /* 中性 😐 */
    {"DEJECTED",     &icon_cat_dejected},      /* 沮丧 😮‍💨 */
    {"KISSY",        &icon_cat_kissy},         /* 撒娇 😘 */
    {"SHY",          &icon_cat_shy},           /* 害羞 😳 */
    {"CALM",         &icon_cat_calm},          /* 沉着 😶 */

    /* legacy emotion names → closest match */
    {"LAUGHING",     &icon_cat_happy},
    {"FUNNY",        &icon_cat_happy},
    {"LOVING",       &icon_cat_excited},
    {"WINK",         &icon_cat_happy},
    {"COOL",         &icon_cat_indifferent},
    {"CONFIDENT",    &icon_cat_indifferent},
    {"EMBARRASSED",  &icon_cat_shy},
    {"SHOCKED",      &icon_cat_fear},
    {"FEARFUL",      &icon_cat_fear},
    {"THINKING",     &icon_cat_calm},
    {"CONFUSED",     &icon_cat_disgust},
    {"RELAXED",      &icon_cat_calm},
    {"SLEEP",        &icon_cat_calm},
    {"SILLY",        &icon_cat_happy},
    {"DELICIOUS",    &icon_cat_excited},
    {"DISAPPOINTED", &icon_cat_dejected},
    {"ANNOYED",      &icon_cat_angry},
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
