/*
**  mod_identicon.c -- Apache identicon module
**
**  Then activate it in Apache's httpd.conf file:
**
**    # httpd.conf
**    LoadModule identicon_module modules/mod_identicon.so
**    IdenticonMemcacheHost   localhost:11211
**    IdenticonMemcacheExpire 300
**    <Location /identicon>
**      SetHandler identicon
**    </Location>
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* httpd */
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "ap_config.h"
#include "apr_strings.h"
#include "util_md5.h"

/* apreq2 */
#include "apreq2/apreq_module_apache2.h"

/* gd */
#include <gd.h>

#ifdef IDENTICON_HAVE_MEMCACHE
/* libmemcached */
#include "memcached.h"
#endif


/* log */
#ifdef AP_IDENTICON_DEBUG_LOG_LEVEL
#define IDENTICON_DEBUG_LOG_LEVEL AP_IDENTICON_DEBUG_LOG_LEVEL
#else
#define IDENTICON_DEBUG_LOG_LEVEL APLOG_DEBUG
#endif

#define _RERR(r, format, args...)                                           \
    ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0,                                \
                  r, "[IDENTICON] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _SERR(s, format, args...)                                           \
    ap_log_error(APLOG_MARK, APLOG_CRIT, 0,                                 \
                 s, "[IDENTICON] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _PERR(p, format, args...)                                            \
    ap_log_perror(APLOG_MARK, APLOG_CRIT, 0,                                 \
                  p, "[IDENTICON] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _RDEBUG(r, format, args...)                       \
    ap_log_rerror(APLOG_MARK, IDENTICON_DEBUG_LOG_LEVEL, 0, \
                  r, "[IDENTICON_DEBUG] %s(%d): "format,    \
                  __FILE__, __LINE__, ##args)
#define _SDEBUG(s, format, args...)                      \
    ap_log_error(APLOG_MARK, IDENTICON_DEBUG_LOG_LEVEL, 0, \
                 s, "[IDENTICON_DEBUG] %s(%d): "format,    \
                 __FILE__, __LINE__, ##args)
#define _PDEBUG(p, format, args...)                       \
    ap_log_perror(APLOG_MARK, IDENTICON_DEBUG_LOG_LEVEL, 0, \
                  p, "[IDENTICON_DEBUG] %s(%d): "format,    \
                  __FILE__, __LINE__, ##args)

#define IDENTICON_CONTENT_TYPE "image/png"
#define IDENTICON_DEFAULT_HASH "098f6bcd4621d373cade4e832627b4f6"
#define IDENTICON_DEFAULT_SIZE 80
#define IDENTICON_IMAGE_SPRITE 128
#define IDENTICON_DEFAULT_MEMCACHE_EXPIRE 0

typedef struct {
    int shape;
    int rotate;
    int red;
    int green;
    int blue;
    int background;
} identicon_shape_t;

typedef struct {
    identicon_shape_t corner;
    identicon_shape_t side;
    identicon_shape_t center;
    gdImagePtr base;
    int sprite;
    int background;
} identicon_image_t;

#ifdef IDENTICON_HAVE_MEMCACHE
typedef struct {
    apr_pool_t *pool;
    char *hosts;
    time_t expire;
    struct memcached_st *memc;
    struct memcached_server_st *servers;
} identicon_server_config_t;
#endif

module AP_MODULE_DECLARE_DATA identicon_module;


static int
identicon_hexdec(char first, char second)
{
    int num = 0;

    if (first >= '0' && first <= '9') {
        first -= '0';
    } else if (first >= 'A' && first <= 'Z') {
        first -= 'A' - 10;
    } else if (first >= 'a' && first <= 'z') {
        first -= 'a' - 10;
    }

    num = num + first;

    if (second != 0) {
        if (second >= '0' && second <= '9') {
            second -= '0';
        } else if (second >= 'A' && second <= 'Z') {
            second -= 'A' - 10;
        } else if (second >= 'a' && second <= 'z') {
            second -= 'a' - 10;
        }
        num = (num * 16) + second;
    }

    return num;
}

static void
identicon_shape(gdImagePtr img, gdPoint *pts, size_t num, int foreground)
{
    if (img == NULL || pts == NULL) {
        return;
    }

    gdImageFilledPolygon(img, pts, num, foreground);
}

static void
identicon_shape_triangle(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[3] = { {0.5 * size, size},
                       {size, 0},
                       {size, size} };
    identicon_shape(img, pts, 3, foreground);
}

static void
identicon_shape_parallelogram(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[4] = { {0.5 * size, 0},
                       {size, 0},
                       {0.5 * size, size},
                       {0, size} };
    identicon_shape(img, pts, 4, foreground);
}

static void
identicon_shape_mouse_ears(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[5] = { {0.5 * size, 0},
                       {size, 0},
                       {size, size},
                       {0.5 * size, size},
                       {size, 0.5 * size} };
    identicon_shape(img, pts, 5, foreground);
}

static void
identicon_shape_ribbon(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[5] = { {0, 0.5 * size},
                       {0.5 * size, 0},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size} };
    identicon_shape(img, pts, 5, foreground);
}

static void
identicon_shape_sails(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[5] = { {0, 0.5 * size},
                      {size, 0},
                      {size, size},
                      {0, size},
                      {size, 0.5 * size} };
    identicon_shape(img, pts, 5, foreground);
}

static void
identicon_shape_fins(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[5] = { {size, 0},
                       {size, size},
                       {0.5 * size, size},
                       {size, 0.5 * size},
                       {0.5 * size, 0.5 * size} };
    identicon_shape(img, pts, 5, foreground);
}

static void
identicon_shape_beak(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[6] = { {0, 0},
                       {size, 0},
                       {size, 0.5 * size},
                       {0, 0},
                       {0.5 * size, size},
                       {0, size} };
    identicon_shape(img, pts, 6, foreground);
}

static void
identicon_shape_chevron(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[6] = { {0, 0},
                       {0.5 * size, 0},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0, size},
                       {0.5 * size, 0.5 * size} };
    identicon_shape(img, pts, 6, foreground);
}

static void
identicon_shape_fish(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[7] = { {0.5 * size, 0},
                       {0.5 * size, 0.5 * size},
                       {size, 0.5 * size},
                       {size, size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size},
                       {0, 0.5 * size} };
    identicon_shape(img, pts, 7, foreground);
}

static void
identicon_shape_kite(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[7] = { {0, 0},
                       {size, 0},
                       {0.5 * size, 0.5 * size},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size},
                       {0, size} };
    identicon_shape(img, pts, 7, foreground);
}

static void
identicon_shape_trough(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[7] = { {0, 0.5 * size},
                       {0.5 * size, size},
                       {size, 0.5 * size},
                       {0.5 * size, 0},
                       {size, 0},
                       {size, size},
                       {0, size} };
    identicon_shape(img, pts, 7, foreground);
}

static void
identicon_shape_rays(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[7] = { {0.5 * size, 0},
                       {size, 0},
                       {size, size},
                       {0.5 * size, size},
                       {size, 0.75 * size},
                       {0.5 * size, 0.5 * size},
                       {size, 0.25 * size} };
    identicon_shape(img, pts, 7, foreground);
}

static void
identicon_shape_double_rhombus(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[8] = { {0, 0.5 * size},
                       {0.5 * size, 0},
                       {0.5 * size, 0.5 * size},
                       {size, 0},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size},
                       {0, size} };
    identicon_shape(img, pts, 8, foreground);
}

static void
identicon_shape_crown(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[9] = { {0, 0},
                       {size, 0},
                       {size, size},
                       {0, size},
                       {size, 0.5 * size},
                       {0.5 * size, 0.25 * size},
                       {0.5 * size, 0.75 * size},
                       {0, 0.5 * size},
                       {0.5 * size, 0.25 * size} };
    identicon_shape(img, pts, 9, foreground);
}

static void
identicon_shape_radioactive(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[9] = { {0, 0.5 * size},
                       {0.5 * size, 0.5 * size},
                       {0.5 * size, 0},
                       {size, 0},
                       {0.5 * size, 0.5 * size},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size},
                       {0, size} };
    identicon_shape(img, pts, 9, foreground);
}

static void
identicon_shape_tiles(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[9] = { {0, 0},
                       {size, 0},
                       {0.5 * size, 0.5 * size},
                       {0.5 * size, 0},
                       {0, 0.5 * size},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0.5 * size, 0.5 * size},
                       {0, size}};
    identicon_shape(img, pts, 9, foreground);
}

static void
identicon_shape_fill(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[4] = { {0, 0},
                       {size, 0},
                       {size, size},
                       {0, size} };
    identicon_shape(img, pts, 4, foreground);
}

static void
identicon_shape_diamond(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[4] = { {0.5 * size, 0},
                       {size, 0.5 * size},
                       {0.5 * size, size},
                       {0, 0.5 * size} };
    identicon_shape(img, pts, 4, foreground);
}

static void
identicon_shape_reverse_diamond(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[9] = { {0, 0},
                       {size, 0},
                       {size, size},
                       {0, size},
                       {0, 0.5 * size},
                       {0.5 * size, size},
                       {size, 0.5 * size},
                       {0.5 * size, 0},
                       {0, 0.5 * size} };
    identicon_shape(img, pts, 9, foreground);
}

static void
identicon_shape_cross(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[12] = { {0.25 * size, 0},
                        {0.75 * size, 0},
                        {0.5 * size, 0.5 * size},
                        {size, 0.25 * size},
                        {size, 0.75 * size},
                        {0.5 * size, 0.5 * size},
                        {0.75 * size, size},
                        {0.25 * size, size},
                        {0.5 * size, 0.5 * size},
                        {0, 0.75 * size},
                        {0, 0.25 * size},
                        {0.5 * size, 0.5 * size} };
    identicon_shape(img, pts, 12, foreground);
}

static void
identicon_shape_morning_star(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[8] = { {0, 0},
                       {0.5 * size, 0.25 * size},
                       {size, 0},
                       {0.75 * size, 0.5 * size},
                       {size, size},
                       {0.5 * size, 0.75 * size},
                       {0, size},
                       {0.25 * size, 0.5 * size} };
    identicon_shape(img, pts, 8, foreground);
}

static void
identicon_shape_small_square(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[4] = { {0.33 * size, 0.33 * size},
                       {0.67 * size, 0.33 * size},
                       {0.67 * size, 0.67 * size},
                       {0.33 * size, 0.67 * size} };
    identicon_shape(img, pts, 4, foreground);
}

static void
identicon_shape_checkerboard(gdImagePtr img, int size, int foreground)
{
    gdPoint pts[19] = { {0, 0},
                        {0.33 * size, 0},
                        {0.33 * size, 0.33 * size},
                        {0.66 * size, 0.33 * size},
                        {0.67 * size, 0},
                        {size, 0},
                        {size, 0.33 * size},
                        {0.67 * size, 0.33 * size},
                        {0.67 * size, 0.67 * size},
                        {size, 0.67 * size},
                        {0.67 * size, size},
                        {0.67 * size, 0.67 * size},
                        {0.33 * size, 0.67 * size},
                        {0.33 * size, size},
                        {0, size},
                        {0, 0.67 * size},
                        {0.33 * size, 0.67 * size},
                        {0.33 * size, 0.33 * size},
                        {0, 0.33 * size}};
    identicon_shape(img, pts, 19, foreground);
}

static int
identicon_image_init(identicon_image_t *image, char *hash)
{
    image->sprite = IDENTICON_IMAGE_SPRITE;

    image->base = gdImageCreateTrueColor(image->sprite * 3, image->sprite * 3);
    if (image->base == NULL) {
        return -1;
    }
    gdImageSetAntiAliased(image->base, 1);

    //white as background
    image->background = gdImageColorAllocate(image->base, 0xff, 0xff, 0xff);
    gdImageFilledRectangle(image->base, 0, 0,
                           image->sprite, image->sprite, image->background);

    //shape, rotate, color
    image->corner.shape = identicon_hexdec(hash[0], 0);
    image->side.shape = identicon_hexdec(hash[1], 0);
    image->center.shape = identicon_hexdec(hash[2], 0) & 7;

    image->corner.rotate = identicon_hexdec(hash[3], 0) & 3;
    image->side.rotate = identicon_hexdec(hash[4], 0) & 3;

    image->center.background = identicon_hexdec(hash[5], 0) % 2;

    image->corner.red = identicon_hexdec(hash[6], hash[7]);
    image->corner.green = identicon_hexdec(hash[8], hash[9]);
    image->corner.blue = identicon_hexdec(hash[10], hash[11]);

    image->side.red = identicon_hexdec(hash[12], hash[13]);
    image->side.green = identicon_hexdec(hash[14], hash[15]);
    image->side.blue = identicon_hexdec(hash[16], hash[17]);

    return 0;
}

static void
identicon_image_destroy(identicon_image_t *image)
{
    gdImageDestroy(image->base);
}

static gdImagePtr
identicon_image_rotate90(gdImagePtr src, size_t num)
{
    size_t x, y, angle = 90;
    gdImagePtr rotate;

    if (num % 2 == 0) {
        x = gdImageSX(src);
        y = gdImageSY(src);
    } else {
        x = gdImageSY(src);
        y = gdImageSX(src);
    }

    rotate = gdImageCreateTrueColor(x, y);
    if (rotate == NULL) {
        return src;
    }

    gdImageCopyRotated(rotate, src, gdImageSX(rotate)/2, gdImageSY(rotate)/2,
                       0, 0, gdImageSX(src), gdImageSY(src), angle * num);

    gdImageDestroy(src);

    return rotate;
}

static gdImagePtr
identicon_generate_circle(identicon_image_t *image, identicon_shape_t shape)
{
    gdImagePtr img;
    int foreground, background;

    img = gdImageCreateTrueColor(image->sprite, image->sprite);
    if (img == NULL) {
        return NULL;
    }
    gdImageSetAntiAliased(img, 1);

    foreground = gdImageColorAllocate(img, shape.red, shape.green, shape.blue);
    background = gdImageColorAllocate(img, 0xff, 0xff, 0xff);

    gdImageFilledRectangle(img, 0, 0, image->sprite, image->sprite, background);

    /* shape */
    switch (shape.shape) {
        case 0:
            identicon_shape_triangle(img, image->sprite, foreground);
            break;
        case 1:
            identicon_shape_parallelogram(img, image->sprite, foreground);
            break;
        case 2:
            identicon_shape_mouse_ears(img, image->sprite, foreground);
            break;
        case 3:
            identicon_shape_ribbon(img, image->sprite, foreground);
            break;
        case 4:
            identicon_shape_sails(img, image->sprite, foreground);
            break;
        case 5:
            identicon_shape_fins(img, image->sprite, foreground);
            break;
        case 6:
            identicon_shape_beak(img, image->sprite, foreground);
            break;
        case 7:
            identicon_shape_chevron(img, image->sprite, foreground);
            break;
        case 8:
            identicon_shape_fish(img, image->sprite, foreground);
            break;
        case 9:
            identicon_shape_kite(img, image->sprite, foreground);
            break;
        case 10:
            identicon_shape_trough(img, image->sprite, foreground);
            break;
        case 11:
            identicon_shape_rays(img, image->sprite, foreground);
            break;
        case 12:
            identicon_shape_double_rhombus(img, image->sprite, foreground);
            break;
        case 13:
            identicon_shape_crown(img, image->sprite, foreground);
            break;
        case 14:
            identicon_shape_radioactive(img, image->sprite, foreground);
            break;
        default:
            identicon_shape_tiles(img, image->sprite, foreground);
            break;
    }

    /* rotate 90 */
    if (shape.rotate > 0) {
        img = identicon_image_rotate90(img, shape.rotate);
    }

    return img;
}

static int
identicon_generate_corner(identicon_image_t *image)
{
    gdImagePtr img;

    img = identicon_generate_circle(image, image->corner);
    if (img == NULL) {
        return -1;
    }

    /* copy */
    gdImageCopy(image->base, img,
                0, 0, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                0, (image->sprite)*2, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                (image->sprite)*2, (image->sprite)*2, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                (image->sprite)*2, 0, 0, 0,
                image->sprite, image->sprite);

    gdImageDestroy(img);

    return 0;
}

static int
identicon_generate_side(identicon_image_t *image)
{
    gdImagePtr img;

    img = identicon_generate_circle(image, image->side);
    if (img == NULL) {
        return -1;
    }

    /* copy */
    gdImageCopy(image->base, img,
                image->sprite, 0, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                0, image->sprite, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                image->sprite, (image->sprite)*2, 0, 0,
                image->sprite, image->sprite);

    img = identicon_image_rotate90(img, 1);
    gdImageCopy(image->base, img,
                (image->sprite)*2, image->sprite, 0, 0,
                image->sprite, image->sprite);

    gdImageDestroy(img);

    return 0;
}

static int
identicon_generate_center(identicon_image_t *image)
{
    int foreground, background;
    gdImagePtr img;

    img = gdImageCreateTrueColor(image->sprite, image->sprite);
    if (img == NULL) {
        return -1;
    }
    gdImageSetAntiAliased(img, 1);

    foreground = gdImageColorAllocate(img, image->corner.red,
                                      image->corner.green, image->corner.blue);

    if (image->center.background > 0 &&
        (abs(image->corner.red - image->side.red) > 127 ||
         abs(image->corner.green - image->side.green) > 127 ||
         abs(image->corner.blue - image->side.blue) > 127)) {
        background = gdImageColorAllocate(img, image->side.red,
                                          image->side.green, image->side.blue);
    } else {
        background = gdImageColorAllocate(img, 0xff, 0xff, 0xff);
    }

    gdImageFilledRectangle(img, 0, 0, image->sprite, image->sprite, background);

    /* shape */
    switch (image->center.shape) {
        case 1:
            identicon_shape_fill(img, image->sprite, foreground);
            break;
        case 2:
            identicon_shape_diamond(img, image->sprite, foreground);
            break;
        case 3:
            identicon_shape_reverse_diamond(img, image->sprite, foreground);
            break;
        case 4:
            identicon_shape_cross(img, image->sprite, foreground);
            break;
        case 5:
            identicon_shape_morning_star(img, image->sprite, foreground);
            break;
        case 6:
            identicon_shape_small_square(img, image->sprite, foreground);
            break;
        case 7:
            identicon_shape_checkerboard(img, image->sprite, foreground);
            break;
        default:
            break;
    }

    gdImageCopy(image->base, img, image->sprite, image->sprite, 0, 0,
                image->sprite, image->sprite);
    gdImageDestroy(img);

    return 0;
}

static int
identicon_image_resize(identicon_image_t *image, int width, int height)
{
    if (gdImageSX(image->base) != width || gdImageSX(image->base) != height) {
        gdImagePtr img = gdImageCreateTrueColor(width, height);
        if (img == NULL) {
            return -1;
        }

        gdImageCopyResized(img, image->base, 0, 0, 0, 0, width, height,
                           gdImageSX(image->base), gdImageSY(image->base));
        gdImageDestroy(image->base);
        image->base = img;
    }

    return 0;
}

static void
identicon_image_transparent(identicon_image_t *image)
{
    gdImageColorTransparent(image->base, image->background);
}

#ifdef IDENTICON_HAVE_MEMCACHE
static apr_status_t
memcache_cleanup(void *parms)
{
    identicon_server_config_t *cfg = (identicon_server_config_t *)parms;

    if (!cfg) {
        return APR_SUCCESS;
    }

    /* memcached cleanup */
    if (cfg->servers) {
        memcached_server_list_free(cfg->servers);
        cfg->servers = NULL;
    }

    if (cfg->memc) {
        memcached_free(cfg->memc);
        cfg->memc = NULL;
    }

    if (cfg->pool) {
        apr_pool_clear(cfg->pool);
        cfg->pool = NULL;
    }

    return APR_SUCCESS;
}

struct memcached_st *
memcache_init(request_rec *r, time_t *expire)
{
    identicon_server_config_t *cfg;

    cfg = ap_get_module_config(r->server->module_config, &identicon_module);

    if (!cfg->hosts) {
        return NULL;
    }

    *expire = cfg->expire;

    if (cfg->memc) {
        return cfg->memc;
    }

    cfg->memc = memcached_create(NULL);
    if (!cfg->memc) {
        return NULL;
    }

    if (cfg->servers) {
        memcached_server_list_free(cfg->servers);
        cfg->servers = NULL;
    }

    cfg->servers = memcached_servers_parse(cfg->hosts);
    if (!cfg->servers) {
        memcached_free(cfg->memc);
        cfg->memc = NULL;
        return NULL;
    }

    if (memcached_server_push(cfg->memc, cfg->servers) != MEMCACHED_SUCCESS) {
        memcached_server_list_free(cfg->servers);
        memcached_free(cfg->memc);
        cfg->servers = NULL;
        cfg->memc = NULL;
        return NULL;
    }

    /* cleanup */
    apr_pool_cleanup_register(cfg->pool, (void *)cfg, memcache_cleanup,
                              apr_pool_cleanup_null);

    return cfg->memc;
}

static char *
memcache_get(struct memcached_st *memc, request_rec *r, int *length)
{
    char *key, *ret = NULL;
    size_t key_len, ret_len;
    memcached_return rc;

    if (!memc || !r) {
        return NULL;
    }

    key = ap_md5(r->pool, (const unsigned char *)r->unparsed_uri);
    key_len = strlen(key);

    ret = memcached_get(memc, key, key_len, &ret_len, (uint16_t)0, &rc);
    if (rc != MEMCACHED_SUCCESS) {
        if (ret) {
            free(ret);
        }
        *length = 0;
        return NULL;
    }

    *length = (int)ret_len;

    return ret;
}

static apr_status_t
memcache_set(struct memcached_st *memc, request_rec *r,
             char *data, int length, time_t expire)
{
    char *key;
    size_t key_len;
    time_t now;
    struct tm *lt;

    if (!memc || !r) {
        return APR_EGENERAL;
    }

    key = ap_md5(r->pool, (const unsigned char *)r->unparsed_uri);
    key_len = strlen(key);

    if (memcached_set(memc, key, key_len, data, length,
                      expire, (uint16_t)0) != MEMCACHED_SUCCESS) {
        return APR_EGENERAL;
    }

    return APR_SUCCESS;
}
#endif

/* content handler */
static int
identicon_handler(request_rec *r)
{
    char *user = NULL, *param_s = NULL, *trans = NULL;
    size_t size = 0;
    apreq_handle_t *apreq;
    apr_table_t *params;
    char *data = NULL;
    int length;
    identicon_image_t image;
#ifdef IDENTICON_HAVE_MEMCACHE
    struct memcached_st *memc = NULL;
    time_t expire = 0;
#endif

    if (strcmp(r->handler, "identicon")) {
        return DECLINED;
    }

    if (r->header_only) {
        return OK;
    }

    /* set contest type */
    r->content_type = IDENTICON_CONTENT_TYPE;

#ifdef IDENTICON_HAVE_MEMCACHE
    /* memcache init */
    memc = memcache_init(r, &expire);

    /* memcache get cache */
    data = memcache_get(memc, r, &length);
    if (data) {
        ap_rwrite(data, length, r);
        free(data);
        return OK;
    }
#endif

    /* get parameter */
    apreq = apreq_handle_apache2(r);
    params = apreq_params(apreq, r->pool);
    if (params) {
        user = (char *)apreq_params_as_string(r->pool, params,
                                              "u", APREQ_JOIN_AS_IS);
        param_s = (char *)apreq_params_as_string(r->pool, params,
                                                 "s", APREQ_JOIN_AS_IS);
        trans = (char *)apr_table_get(params, "t");
        if (param_s) {
            size = (size_t)atol(param_s);
        }
    }

    if (!user || strlen(user) < 20) {
        user = IDENTICON_DEFAULT_HASH;
    }

    if (size == 0) {
        size = IDENTICON_DEFAULT_SIZE;
    }

    if (identicon_image_init(&image, user) != 0) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (identicon_generate_corner(&image) != 0) {
        identicon_image_destroy(&image);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (identicon_generate_side(&image) != 0) {
        identicon_image_destroy(&image);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (identicon_generate_center(&image) != 0) {
        identicon_image_destroy(&image);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (identicon_image_resize(&image, size, size) != 0) {
        identicon_image_destroy(&image);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (trans != NULL) {
        identicon_image_transparent(&image);
    }

    /* output */
    data = (char *)gdImagePngPtr(image.base, &length);
    if (!data) {
        identicon_image_destroy(&image);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    ap_rwrite(data, length, r);

#ifdef IDENTICON_HAVE_MEMCACHE
    /* memcache set cache */
    memcache_set(memc, r, data, length, expire);
#endif

    gdFree(data);

    identicon_image_destroy(&image);

    return OK;
}

#ifdef IDENTICON_HAVE_MEMCACHE
static void *
identicon_create_server_config(apr_pool_t *p, server_rec *s)
{
    identicon_server_config_t *cfg;

    cfg = apr_pcalloc(p, sizeof(identicon_server_config_t));

    memset(cfg, 0, sizeof(identicon_server_config_t));

    apr_pool_create(&cfg->pool, p);

    cfg->hosts = NULL;
    cfg->expire = IDENTICON_DEFAULT_MEMCACHE_EXPIRE;
    cfg->memc = NULL;
    cfg->servers = NULL;

    return (void *)cfg;
}
#endif

/*
static void *
identicon_merge_server_config(apr_pool_t *p, void *base_srv, void *override_srv)
{
    identicon_server_config_t *cfg, *base, *override;

    cfg = apr_pcalloc(p, sizeof(identicon_server_config_t));
    base = (identicon_server_config_t *)base_srv;
    override  = (identicon_server_config_t *)override_srv;

    cfg->pool = base->pool;

    if (override->hosts) {
        cfg->hosts = override->hosts;
    } else {
        cfg->hosts = NULL;
    }

    cfg->expire = override->expire;

    return (void *)cfg;
}
*/

#ifdef IDENTICON_HAVE_MEMCACHE
static const char *
identicon_memcache_set_host(cmd_parms *parms, void *conf, char *arg)
{
    identicon_server_config_t *cfg;

    if (strlen(arg) == 0) {
        return "MemcacheHost argument must be a string representing a hosts.";
    }

    cfg = (identicon_server_config_t *)ap_get_module_config(
        parms->server->module_config, &identicon_module);

    cfg->hosts = apr_pstrdup(parms->pool, arg);

    return NULL;
}

static const char *
identicon_memcache_set_expire(cmd_parms *parms, void *conf, char *arg)
{
    identicon_server_config_t *cfg;
    int expire;

    if (sscanf(arg, "%d", &expire) != 1 || expire < 0) {
        return "MemcacheExpire must be an integer representing the expire.";
    }

    cfg =(identicon_server_config_t *)ap_get_module_config(
        parms->server->module_config, &identicon_module);

    cfg->expire = (time_t)expire;

    return NULL;
}
#endif

static const command_rec
identicon_cmds[] = {
#ifdef IDENTICON_HAVE_MEMCACHE
    AP_INIT_TAKE1("IdenticonMemcacheHost",
                  (const char*(*)())(identicon_memcache_set_host), NULL,
                  RSRC_CONF, "identicon memcache hosts"),
    AP_INIT_TAKE1("IdenticonMemcacheExpire",
                  (const char*(*)())(identicon_memcache_set_expire), NULL,
                  RSRC_CONF, "identicon memcache expire"),
#endif
    {NULL}
};

static void
identicon_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(identicon_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA identicon_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,                           /* create per-dir    config structures */
    NULL,                           /* merge  per-dir    config structures */
#ifdef IDENTICON_HAVE_MEMCACHE
    identicon_create_server_config, /* create per-server config structures */
#else
    NULL,
#endif
    NULL,                           /* merge  per-server config structures */
    /* identicon_merge_server_config, */
    identicon_cmds,                 /* table of config file commands       */
    identicon_register_hooks        /* register hooks                      */
};
