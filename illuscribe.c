/*
 * File: illuscribe.c
 * A tool to present slideshows from plaintext files.
 * --------------------
 * Author: Mason Armand
 * Contributors:
 * Date Created: May 17, 2023
 * Last Modified: May 22, 2023
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef struct Slide Slide;
typedef struct Box Box;
typedef struct Text Text;
typedef struct Image Image;

typedef enum {
        FONT_TITLE,
        FONT_NORMAL,
        FONT_SMALL,
        FONT_HUGE
} FontSize;

typedef enum {
        ELEMENT_TYPE_SLIDE,
        ELEMENT_TYPE_BOX,
        ELEMENT_TYPE_TEXT,
        ELEMENT_TYPE_IMAGE
} ElementType;

typedef enum {
        TEXT_ALIGN_CENTER,
        TEXT_ALIGN_LEFT,
        TEXT_ALIGN_RIGHT
} TextAlignmentType;

typedef enum {
        STACK_HORIZONTAL,
        STACK_VERTICAL
} StackType;

typedef struct {
        ElementType type;
        union {
                Slide* slide;
                Box* box;
                Text* text;
                Image* image;
        } element;
} SlideElement;

struct Slide {
        ElementType type;
        char* name;
        bool visible;
        unsigned int element_count;
        SlideElement** elements;
};

struct Box {
        ElementType type;
        TextAlignmentType text_align;
        StackType stack_type;
        char* name;
        unsigned int element_count;
        SlideElement** elements;

        float x, y, width, height;
};

struct Text {
        ElementType type;
        FontSize font_size;
        char* content;

        float x, y, size;
};

struct Image {
        ElementType type;
        XImage* ximage;
        XRenderPictFormat* xrenderformat;
        Picture src;
        Picture dest;
        char* filename;
        unsigned char* data;
        int width;
        int height;
        int channels;
        float rheight;
        float rwidth;
        float x, y;
};

typedef struct {
        Slide** slides;
        unsigned int count;
} SlideList;


typedef void (*KeywordHandler)(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);

typedef struct {
        const char* keyword;
        KeywordHandler handler;
} KeywordMapEntry;


void parse_slideshow(char* filename, SlideList* list);

void render_endslide(Display* dpy, Window window, int screen);
void render_slideshow(int width, int height, SlideList list);
void change_slide(Display* dpy, Window window, SlideList list, unsigned int* slide_idx, int screen, int amount);
void update_title(Display* dpy, Window window, SlideList list, unsigned int slide_idx);
char* get_top_text(Slide slide);
void render_slide(Slide slide, Display* dpy, Window window, int screen);
void render_box(Box box, unsigned int window_width, unsigned int window_height, Display* dpy, Window window, int screen);
void render_text(Text text, Display* dpy, int screen, int box_x, int box_y, int box_w, int box_h, int window_width);
void render_image(Image image, Display* dpy, Window window, int screen, int box_x, int box_y, int box_w, int box_h);

void skip_templates(SlideList list, unsigned int* slide_idx);
void apply_layout(Slide* slide, Display* dpy, Window window);
void position_elements(Box* box, Display* dpy, Window window);
float get_char_width(const char c, FontSize size, Display* dpy);
float get_strtext_width(char* str, FontSize size, Display* dpy);
float get_text_width(Text text, Display* dpy);
float get_line_height(Text text, int box_height);
float get_font_ascent(Text text, int box_height);
void apply_word_wrap(Display* dpy, Window window, Box* box);
float calculate_vbox_height(Display* dpy, Window window, Box* box);
void create_slide(Slide** slide, char* name, bool visible);
void slide_list_init(SlideList *slide_list);
void slide_list_append(SlideList *slide_list, Slide *slide);

void slide_list_free(SlideList *slide_list);
void free_slide(Slide* slide);
void free_box(Box* box);
void free_text(Text* text);
void free_image(Image* image);

void add_element_to_slide(Slide* slide, SlideElement* element);
void add_element_to_box(Box* box, SlideElement* element);
void add_element_to_box_at_index(Box* box, SlideElement* element, unsigned int index);

Slide* copy_slide(Slide* slide);
Box* copy_box(Box* box);
Text* copy_text(Text* text);
Image* copy_image(Image* image);

Slide* find_slide_by_name(char* name, SlideList list);
SlideElement* find_element_by_name(char* name, Slide* slide);

void check_syntax(char** check_args, unsigned int check_len, char* expect, unsigned int line_num);
bool is_number(char *str, bool* is_negative);
bool is_string(char* str);
char* remove_quotes(const char* str);
void swap_rb_channels(unsigned char* img_data, int width, int height);
void create_text(Text** text, char* content, FontSize font_size);
void create_image(Image** image, char* filename);
void create_box(Box** box, char* name, StackType stack, TextAlignmentType alignment);
char** split_str(char* str, const char* delim, unsigned int* length_return);
void free_split_str(char** split_str, unsigned int len);
void trim_str(char* str);

/* handler functions */
void handle_slide(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_template(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_box(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_uses(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_text(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_image(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);
void handle_define(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num);

KeywordMapEntry keyword_map[] = {
        {"slide", handle_slide},
        {"template", handle_template},
        {"box", handle_box},
        {"uses", handle_uses},
        {"text", handle_text},
        {"image", handle_image},
        {"define", handle_define},
};

char global_font_name[] = "Serif";
double global_huge_font_size = 40.0f;
double global_title_font_size = 25.0f;
double global_normal_font_size = 18.0f;
double global_small_font_size = 15.0f;

XftFont* global_fonts[4];
XftDraw* draw;
XftColor color;
XftColor color_white;


int main(int argc, char** argv)
{
        SlideList slide_list;

        if (argc < 2) {
                fprintf(stderr, "Usage: %s <slideshow file>\n", argv[0]);
                exit(1);
        }

        slide_list_init(&slide_list);
        parse_slideshow(argv[1], &slide_list);
        if (argc == 4) {
                int width = atoi(argv[2]);
                int height = atoi(argv[3]);
                render_slideshow(width, height, slide_list);
        }
        else {
                render_slideshow(854, 480, slide_list); /* default to 16:9 aspect ratio */
        }
        slide_list_free(&slide_list);

        return 0;
}


void parse_slideshow(char* filename, SlideList* list)
{
        Slide* current_slide = NULL;
        Box* current_box = NULL;
        FILE* file = fopen(filename, "r");
        char line[1024];
        int line_num = 1;

        if (!file) {
                fprintf(stderr, "Error opening file: %s\n", filename);
                exit(1);
        }

        while (fgets(line, sizeof(line), file)) {
                unsigned int len;
                unsigned int i;
                char** split;

                trim_str(line);

                if (strlen(line) <= 1) {
                        line_num ++;
                        continue;
                }

                split = split_str(line, ":,", &len);

                if (len < 1)
                        goto continue_loop;


                for (i = 0; i < sizeof(keyword_map) / sizeof(keyword_map[0]); i++) {
                        if (strcmp(split[0], keyword_map[i].keyword) == 0) {
                                keyword_map[i].handler(*list, &current_slide, &current_box, split, len, line_num);
                                break;
                        }
                }
                if (strstr(split[0], "end") != NULL) {
                        if (current_box != NULL) {
                                current_box = NULL;
                        }
                        else if (current_slide != NULL) {
                                slide_list_append(list, current_slide);
                                current_slide = NULL;
                        }
                        else {
                                fprintf(stderr, "Syntax Error on line %d : Unmatched end keyword.", line_num);
                                exit(1);
                        }
                }

                continue_loop:
                free_split_str(split, len);
                line_num ++;
        }

        fclose(file);
}


void render_slideshow(int window_width, int window_height, SlideList list)
{
        Display* dpy;
        Window window;
        Atom del_window;
        XRenderColor render_color = { 0x0000, 0x0000, 0x0000, 0xFFFF };
        XRenderColor render_color_white = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
        XEvent e;
        int screen;
        int last_width = window_width;
        int last_height = window_height;
        unsigned int slide_idx = 0;
        unsigned int i;
        bool running = true;

        dpy = XOpenDisplay(NULL);
        if (!dpy) {
                fprintf(stderr, "Failed to open X display.\n");
                exit(1);
        }

        screen = DefaultScreen(dpy);
        window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     10, 10, window_width, window_height,
                                     1, 0x000000, 0xFFFFFF);

        del_window = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(dpy, window, &del_window, 1);
        XSelectInput(dpy, window, ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask);
        XMapWindow(dpy, window);

        global_fonts[FONT_TITLE] = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name,
                                               XFT_SIZE, XftTypeDouble, global_title_font_size, NULL);

        global_fonts[FONT_NORMAL] = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name,
                                                XFT_SIZE, XftTypeDouble, global_normal_font_size, NULL);

        global_fonts[FONT_SMALL] = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name,
                                               XFT_SIZE, XftTypeDouble, global_small_font_size, NULL);

        global_fonts[FONT_HUGE] = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name,
                                               XFT_SIZE, XftTypeDouble, global_huge_font_size, NULL);

        if (!global_fonts[FONT_NORMAL]) {
                fprintf(stderr, "Failed to load font: %s\n", global_font_name);
                exit(1);
        }

        draw = XftDrawCreate(dpy, window, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
        XftColorAllocValue(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &render_color, &color);
        XftColorAllocValue(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &render_color_white, &color_white);

        for (i = 0; i < list.count; i++)
                apply_layout(list.slides[i], dpy, window);

        skip_templates(list, &slide_idx);
        update_title(dpy, window, list, slide_idx);

        while (running) {
                KeySym key;
                XConfigureEvent xce;

                XNextEvent(dpy, &e);

                switch (e.type) {
                case Expose:
                case ConfigureNotify:
                        xce = e.xconfigure;
                        if (xce.width == last_width && xce.height == last_height)
                                break;
                        last_width = xce.width;
                        last_height = xce.height;
                        if (slide_idx >= list.count) {
                                render_endslide(dpy, window, screen);
                                break;
                        }
                        render_slide(*list.slides[slide_idx], dpy, window, screen);
                        break;
                case ButtonPress:
                        if (e.xbutton.button == Button1 || e.xbutton.button == Button4) {
                                change_slide(dpy, window, list, &slide_idx, screen, 1);
                        }
                        else if (e.xbutton.button == Button3 || e.xbutton.button == Button5) {
                                change_slide(dpy, window, list, &slide_idx, screen, -1);
                        }
                        break;
                case KeyPress:
                        key = XLookupKeysym(&e.xkey, 0);
                        if (key == XK_Right || key == XK_Return || key == XK_space) {
                                change_slide(dpy, window, list, &slide_idx, screen, 1);
                        }
                        else if (key == XK_Left) {
                                change_slide(dpy, window, list, &slide_idx, screen, -1);
                        }
                        else if (key == XK_Escape) {
                                running = false;
                        }
                        break;
                case ClientMessage:
                        running = false;
                        break;
                }
        }
        XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &color);
        XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &color_white);
        XftDrawDestroy(draw);

        XftFontClose(dpy, global_fonts[FONT_TITLE]);
        XftFontClose(dpy, global_fonts[FONT_NORMAL]);
        XftFontClose(dpy, global_fonts[FONT_SMALL]);
        XftFontClose(dpy, global_fonts[FONT_HUGE]);

        XDestroyWindow(dpy, window);
        XCloseDisplay(dpy);
}


void change_slide(Display* dpy, Window window, SlideList list, unsigned int* slide_idx, int screen, int amount)
{
        if (list.count == 0) return;

        if (amount < 0 && *slide_idx > 0 && list.slides[*slide_idx - 1]->visible) {
                *slide_idx += amount;
        }
        else if (amount > 0) {
                *slide_idx += amount;
                if (*slide_idx >= list.count) {
                        render_endslide(dpy, window, screen);
                        update_title(dpy, window, list, *slide_idx);
                        *slide_idx = list.count;
                        return;
                }
        }

        render_slide(*list.slides[*slide_idx], dpy, window, screen);
        update_title(dpy, window, list, *slide_idx);
}


void skip_templates(SlideList list, unsigned int* slide_idx)
{
        if (list.count == 0)
                return;
        while (!list.slides[*slide_idx]->visible) {
                (*slide_idx) ++;
                if (*slide_idx >= list.count) {
                        *slide_idx -= 1;
                        break;
                }
        }
}


void update_title(Display* dpy, Window window, SlideList list, unsigned int slide_idx)
{
        char* title;
        if (slide_idx >= list.count) {
                XStoreName(dpy, window, "End of Presentation.");
                return;
        }
        title = get_top_text(*list.slides[slide_idx]);
        XStoreName(dpy, window, title);
}


char* get_top_text(Slide slide)
{
        unsigned int i, j;
        for (i = 0; i < slide.element_count; i++) {
                Box* box;
                if (slide.elements[i]->type == ELEMENT_TYPE_SLIDE) {
                        Slide* found_slide = slide.elements[i]->element.slide;
                        return get_top_text(*found_slide);
                }
                if (slide.elements[i]->type != ELEMENT_TYPE_BOX) {
                        continue;
                }
                box = slide.elements[i]->element.box;
                for (j = 0; j < box->element_count; j++) {
                        if (box->elements[j]->type == ELEMENT_TYPE_TEXT) {
                                return box->elements[j]->element.text->content;
                        }
                }
        }

        return NULL;
}


void render_endslide(Display* dpy, Window window, int screen)
{
        XWindowAttributes attrs;
        XGlyphInfo extents;
        XftFont* font;
        const char* text = "End of presentation.";
        double font_size;
        int x;
        int y;
        int text_width;
        int text_height;

        XGetWindowAttributes(dpy, window, &attrs);
        font_size = 0.03 * attrs.width;

        XSetWindowBackground(dpy, window, 0x000000);
        XClearWindow(dpy, window);
        font = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name, XFT_SIZE, XftTypeDouble, font_size, NULL);

        XftTextExtents8(dpy, font, (XftChar8 *)text, strlen(text), &extents);
        text_width = extents.width;
        text_height = extents.height;

        x = (attrs.width - text_width) / 2;
        y = (attrs.height + text_height) / 2;

        XftDrawString8(draw, &color_white, font, x, y, (XftChar8 *)text, strlen(text));
        XftFontClose(dpy, font);
}


void render_slide(Slide slide, Display* dpy, Window window, int screen)
{
        unsigned int i;
        unsigned int window_width;
        unsigned int window_height;
        XWindowAttributes attrs;

        XGetWindowAttributes(dpy, window, &attrs);
        window_width = attrs.width;
        window_height = attrs.height;

        XSetWindowBackground(dpy, window, 0xFFFFFF);
        XClearWindow(dpy, window);
        for (i = 0; i < slide.element_count; i++) {
                if (slide.elements[i]->type == ELEMENT_TYPE_BOX) {
                        Box* b = slide.elements[i]->element.box;
                        render_box(*b, window_width, window_height, dpy, window, screen);
                }
                else if (slide.elements[i]->type == ELEMENT_TYPE_SLIDE) {
                        Slide* found_slide = slide.elements[i]->element.slide;
                        render_slide(*found_slide, dpy, window, screen);
                }
        }
}


void render_box(Box box, unsigned int window_width, unsigned int window_height, Display* dpy, Window window, int screen)
{
        unsigned int i;
        int box_x = box.x * window_width;
        int box_y = box.y * window_height;
        int box_width = box.width * window_width;
        int box_height = box.height * window_height;
        /*GC gc;
        XGCValues gcvalues;
        gc = XCreateGC(dpy, window, 0, &gcvalues);
        XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
        XSetForeground(dpy, gc, 0x000000);

        XDrawRectangle(dpy, window, gc, box_x, box_y, box_width, box_height);
        XFreeGC(dpy, gc);*/

        for (i = 0; i < box.element_count; i++) {
                switch(box.elements[i]->type) {
                case ELEMENT_TYPE_TEXT:
                        render_text(*(box.elements[i]->element.text), dpy, screen, box_x, box_y, box_width, box_height, window_width);
                        break;
                case ELEMENT_TYPE_IMAGE:
                        render_image(*(box.elements[i]->element.image), dpy, window, screen, box_x, box_y, box_width, box_height);
                        break;
                default:
                        break;
                }
        }
}


void render_text(Text text, Display* dpy, int screen, int box_x, int box_y, int box_w, int box_h, int window_width)
{
        XftFont* font;
        int text_x = box_x + text.x * box_w;
        int text_y = box_y + text.y * box_h;
        double font_size = text.size * window_width;

        font = XftFontOpen(dpy, screen, XFT_FAMILY, XftTypeString, global_font_name, XFT_SIZE, XftTypeDouble, font_size, NULL);

        XftDrawString8(draw, &color, font, text_x, text_y, (XftChar8 *)text.content, strlen(text.content));
        XftFontClose(dpy, font);
}


void render_image(Image image, Display* dpy, Window window, int screen, int box_x, int box_y, int box_w, int box_h)
{
        int img_x = box_x + image.x * box_w;
        int img_y = box_y + image.y * box_h;
        int img_width = image.rwidth * box_w;
        int img_height = image.rheight * box_h;
        int x;
        int y;

        Pixmap pixmap = XCreatePixmap(dpy, window, img_width, img_height, DefaultDepth(dpy, screen));
        XImage* scaled_ximage = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen), ZPixmap, 0, NULL, img_width, img_height, 32, 0);
        Picture img_pic;

        scaled_ximage->data = (char*) malloc(scaled_ximage->bytes_per_line * img_height);

        for (y = 0; y < img_height; y++) {
                for (x = 0; x < img_width; x++) {
                        int src_x = x * image.width / img_width;
                        int src_y = y * image.height / img_height;
                        long pixel = XGetPixel(image.ximage, src_x, src_y);
                        XPutPixel(scaled_ximage, x, y, pixel);
                }
        }

        XPutImage(dpy, pixmap, DefaultGC(dpy, screen), scaled_ximage, 0, 0, 0, 0, img_width, img_height);
        img_pic = XRenderCreatePicture(dpy, pixmap, image.xrenderformat, 0, NULL);

        XRenderComposite(dpy, PictOpSrc, img_pic, None, image.src, 0, 0, 0, 0, img_x, img_y, img_width, img_height);

        XFreePixmap(dpy, pixmap);
        XRenderFreePicture(dpy, img_pic);
        XDestroyImage(scaled_ximage);
}


void apply_layout(Slide* slide, Display* dpy, Window window)
{
        float total_height = 0;
        int total_hboxes = 0;
        float hbox_width = 0;
        int row_count = 0;
        float cur_x = 0;
        float cur_y = 0;
        int cur_count = 0;
        bool in_row = false;

        unsigned int i;
        unsigned int j;


        /* calculate default box size */
        for (i = 0; i < slide->element_count; i++) {
                Box* box;
                if (slide->elements[i]->type == ELEMENT_TYPE_SLIDE) {
                        Slide* nested_slide = slide->elements[i]->element.slide;
                        apply_layout(nested_slide, dpy, window);
                }
                else if (slide->elements[i]->type != ELEMENT_TYPE_BOX)
                        continue;
                box = slide->elements[i]->element.box;
                if (box->stack_type == STACK_VERTICAL) {
                        box->width = 1.0f;
                        apply_word_wrap(dpy, window, box);
                        box->height = calculate_vbox_height(dpy, window, box) / 1.0f;
                        total_height += box->height;
                }
                else if (box->stack_type == STACK_HORIZONTAL) {
                        box->height = 1.0f;
                        total_hboxes += 1;
                }
        }

        if (total_hboxes > 0) {
                hbox_width = 1.0f / total_hboxes;
        }

        /* adjust sizes of hboxes */
        for (i = 0; i < slide->element_count; i++) {
                Box* box;
                if (slide->elements[i]->type != ELEMENT_TYPE_BOX)
                        continue;
                box = slide->elements[i]->element.box;
                if (box->stack_type == STACK_HORIZONTAL) {
                        if (!in_row) {
                                for (j = i; j < slide->element_count; j++) {
                                        Box* rbox;
                                        if (slide->elements[j]->type != ELEMENT_TYPE_BOX)
                                                continue;
                                        rbox = slide->elements[j]->element.box;
                                        if (rbox->stack_type == STACK_HORIZONTAL)
                                                cur_count++;
                                        if (rbox->stack_type == STACK_VERTICAL || j == slide->element_count - 1) {
                                                hbox_width = cur_count <= 1 ? (1.0f) : (1.0f / cur_count);
                                                cur_count = 0;
                                                in_row = true;
                                                row_count ++;
                                                break;
                                        }
                                }
                        }
                        box->width = hbox_width;
                        apply_word_wrap(dpy, window, box);
                }
                else if (box->stack_type == STACK_VERTICAL) {
                        in_row = false;
                }
        }

        for (i = 0; i < slide->element_count; i++) {
                Box* box;
                if (slide->elements[i]->type != ELEMENT_TYPE_BOX)
                        continue;
                box = slide->elements[i]->element.box;
                if (box->stack_type == STACK_HORIZONTAL) {
                        box->height = (1.0f - total_height) / row_count;
                }
        }

        /* adjust positions */
        for (i = 0; i < slide->element_count; i++) {
                Box* box;
                if (slide->elements[i]->type != ELEMENT_TYPE_BOX)
                        continue;
                box = slide->elements[i]->element.box;
                box->y = cur_y;
                box->x = cur_x;
                position_elements(box, dpy, window);
                if (box->stack_type == STACK_VERTICAL) {
                        cur_y += box->height;
                }
                else if (box->stack_type == STACK_HORIZONTAL) {
                        cur_x += box->width;
                        if (cur_x >= 1.0f) {
                                cur_x = 0;
                                cur_y += box->height;
                        }
                }
        }
}


void position_elements(Box* box, Display* dpy, Window window)
{
        float current_y;
        float padding_percent = 0.025f;
        float padding;
        float box_aspect_ratio;
        unsigned int i;
        int box_height_px;
        XWindowAttributes attr;

        XGetWindowAttributes(dpy, window, &attr);
        box_height_px = box->height * attr.height;
        box_aspect_ratio = (box->width * attr.width) / (box->height * attr.height);
        padding = (attr.width * padding_percent) / (box->width * attr.width);
        current_y = padding;

        for (i = 0; i < box->element_count; i++) {
                if (box->elements[i]->type == ELEMENT_TYPE_TEXT) {
                        Text* text = box->elements[i]->element.text;
                        float text_width;
                        float line_height;

                        text_width = (get_text_width(*text, dpy) / attr.width) / box->width;
                        line_height = get_line_height(*text, box_height_px);

                        if (box->element_count == 1) {
                                /* vertically center text if theres only one text element */
                                text->y = 0.5f + (line_height / 2);
                        }
                        else {
                                text->y = current_y + get_font_ascent(*text, box_height_px);
                                current_y += line_height;
                        }

                        switch (box->text_align) {
                        case TEXT_ALIGN_LEFT:
                                text->x = padding;
                                break;
                        case TEXT_ALIGN_CENTER:
                                text->x = 0.5f - text_width / 2;
                                break;
                        case TEXT_ALIGN_RIGHT:
                                text->x = 1.0f - text_width - padding;
                                break;
                        }

                }
                else if (box->elements[i]->type == ELEMENT_TYPE_IMAGE) {
                        Image* image = box->elements[i]->element.image;
                        int screen = DefaultScreen(dpy);
                        float img_scale = 0.9f;
                        float img_aspect_ratio = (float)image->width / image->height;


                        image->xrenderformat = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, 0));
                        image->src = XRenderCreatePicture(dpy, window, image->xrenderformat, 0, NULL);
                        image->ximage = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen), ZPixmap, 0, (char*)image->data, image->width, image->height, 32, 0);

                        image->rwidth = img_scale;
                        image->rheight = img_scale / img_aspect_ratio;

                        image->rheight = image->rheight * box_aspect_ratio;

                        if (image->rheight > box->height - current_y) {
                                image->rheight = box->height - current_y;
                                image->rwidth = image->rheight * img_aspect_ratio / box_aspect_ratio;
                        }

                        if (box->element_count == 1) {
                                /* vertically center image if theres only one text element */
                                image->y = 0.5f - (image->rheight / 2);
                        }
                        else {
                                image->y = current_y;
                        }

                        current_y += image->rheight;

                        switch (box->text_align) {
                        case TEXT_ALIGN_LEFT:
                                image->x = padding;
                                break;
                        case TEXT_ALIGN_CENTER:
                                image->x = 0.5f - image->rwidth / 2;
                                break;
                        case TEXT_ALIGN_RIGHT:
                                image->x = 1.0f - image->rwidth - padding;
                                break;
                        }
                }
        }
}


float get_char_width(const char c, FontSize size, Display* dpy)
{
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, global_fonts[size], (FcChar8*) &c, 1, &extents);
        return extents.xOff;
}


float get_strtext_width(char* str, FontSize size, Display* dpy)
{
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, global_fonts[size], (FcChar8*) str, strlen(str), &extents);
        return extents.xOff;
}


float get_text_width(Text text, Display* dpy)
{
        XGlyphInfo extents;
        int width = 0;
        unsigned int i;
        for (i = 0; i < strlen(text.content); i++) {
                XftTextExtentsUtf8(dpy, global_fonts[text.font_size], (FcChar8*) &text.content[i], 1, &extents);
                width += extents.xOff;
        }
        return width;
}


float get_line_height(Text text, int box_height)
{
        return (float) global_fonts[text.font_size]->height / (float)box_height;
}


float get_font_ascent(Text text, int box_height)
{
        return (float)global_fonts[text.font_size]->ascent / (float)box_height;
}


void apply_word_wrap(Display* dpy, Window window, Box* box)
{
        float current_y = 0.0f;
        float padding_percent = 0.025f;
        float padding = 0.0f;
        unsigned int i;

        XWindowAttributes attr;
        XGetWindowAttributes(dpy, window, &attr);
        padding = (attr.width * padding_percent) / (box->width * attr.width);
        current_y = padding;

        for (i = 0; i < box->element_count; i++) {
                if (box->elements[i]->type == ELEMENT_TYPE_TEXT) {
                        Text* text = box->elements[i]->element.text;
                        float line_height;
                        float slide_text_width;

                        switch (text->font_size) {
                        case FONT_HUGE:
                                text->size = (global_huge_font_size / attr.width);
                                break;
                        case FONT_TITLE:
                                text->size = (global_title_font_size / attr.width);
                                break;
                        case FONT_NORMAL:
                                text->size = (global_normal_font_size / attr.width);
                                break;
                        case FONT_SMALL:
                                text->size = (global_small_font_size / attr.width);
                                break;
                        }

                        slide_text_width = ((get_text_width(*text, dpy) / attr.width) / 1.0f) + padding;

                        if (slide_text_width > box->width && strstr(text->content, " ") != NULL) {
                                unsigned int split_len = 0;
                                unsigned int j;
                                unsigned int break_idx = 0;
                                unsigned int new_len = 0;
                                unsigned int next_len = 0;
                                float width = 0.0;
                                float max_width = 0.0;
                                float space_width = (get_char_width(' ', text->font_size, dpy) / attr.width) / 1.0f;
                                char** split_text = split_str(text->content, " ", &split_len);
                                char* new_line = NULL;
                                char* next_line = NULL;
                                FontSize fn = text->font_size;

                                for (j = 0; j < split_len; j++) {
                                        float temp_width = (get_strtext_width(split_text[j], text->font_size, dpy) / attr.width) / 1.0f;
                                        if (temp_width > max_width) {
                                                max_width = temp_width;
                                        }
                                }

                                if (max_width > box->width) {
                                        fprintf(stderr, "Error: Single word in text is wider than box width. In box: %s\n", box->name);
                                        exit(1);
                                }

                                for (j = 0; j < split_len; j++) {
                                        if (j > 0)
                                                break_idx = j - 1;
                                        width += (get_strtext_width(split_text[j], text->font_size, dpy) / attr.width) / 1.0f;
                                        width += space_width;
                                        if (width >= box->width)
                                                break;
                                }

                                /* create new line of text up to the break point */
                                for (j = 0; j < break_idx; j++)
                                        new_len += strlen(split_text[j]) + 1;

                                new_line = malloc(new_len + 1);
                                new_line[0] = '\0';

                                for (j = 0; j < break_idx; j++) {
                                        strcat(new_line, split_text[j]);
                                        strcat(new_line, " ");
                                }

                                free_text(box->elements[i]->element.text);
                                create_text(&box->elements[i]->element.text, new_line, fn);

                                /* create the remaining line of text */
                                for (j = break_idx; j < split_len; j++)
                                        next_len += strlen(split_text[j]) + 1;

                                /* If theres a line of text below the current one, the remaining text will get combined with it */
                                if (i + 1 < box->element_count && box->elements[i+1]->type == ELEMENT_TYPE_TEXT) {
                                        Text* next_text = box->elements[i+1]->element.text;
                                        next_len += strlen(next_text->content);
                                        next_line = malloc(next_len + 1);
                                        next_line[0] = '\0';
                                        for (j = break_idx; j < split_len; j++) {
                                                strcat(next_line, split_text[j]);
                                                strcat(next_line, " ");
                                        }
                                        strcat(next_line, next_text->content);
                                        free_text(box->elements[i+1]->element.text);
                                        create_text(&(box->elements[i+1]->element.text), next_line, fn);
                                }
                                else {
                                        SlideElement* se = malloc(sizeof(SlideElement));
                                        Text* new_text = NULL;

                                        if (!se) {
                                                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                                                exit(1);
                                        }
                                        se->type = ELEMENT_TYPE_TEXT;

                                        next_line = malloc(next_len + 1);
                                        next_line[0] = '\0';
                                        for (j = break_idx; j < split_len; j++) {
                                                strcat(next_line, split_text[j]);
                                                strcat(next_line, " ");
                                        }

                                        create_text(&new_text, next_line, fn);
                                        se->element.text = new_text;
                                        add_element_to_box_at_index(box, se, i+1);
                                }

                                i--;
                                free_split_str(split_text, split_len);
                                continue;
                        }

                        line_height = get_line_height(*text, attr.height);
                        text->y = current_y + get_font_ascent(*text, attr.height);
                        current_y += line_height;
                }
        }
}


float calculate_vbox_height(Display* dpy, Window window, Box* box)
{
        float current_y = 0.0f;
        float padding_percent = 0.025f;
        float padding = 0.0f;
        unsigned int i;

        XWindowAttributes attr;
        XGetWindowAttributes(dpy, window, &attr);
        padding = (attr.width * padding_percent) / (box->width * attr.width);
        current_y = padding;

        for (i = 0; i < box->element_count; i++) {
                if (box->elements[i]->type == ELEMENT_TYPE_TEXT) {
                        Text* text = box->elements[i]->element.text;
                        float line_height;
                        line_height = get_line_height(*text, attr.height);
                        text->y = current_y + get_font_ascent(*text, attr.height);
                        current_y += line_height;
                }
                else if (box->elements[i]->type == ELEMENT_TYPE_IMAGE) {
                        Image* image = box->elements[i]->element.image;
                        float img_scale = 1.0f;
                        current_y +=  (img_scale * image->height) / image->width;
                }
        }
        return current_y;
}


Slide* copy_slide(Slide* slide)
{
        Slide* new_slide;
        unsigned int i;
        create_slide(&new_slide, strdup(slide->name), true);
        for (i = 0; i < slide->element_count; i++) {
                if (slide->elements[i]->type == ELEMENT_TYPE_BOX) {
                        Box* box = copy_box(slide->elements[i]->element.box);
                        SlideElement* se = malloc(sizeof(SlideElement));
                        if (!se) {
                                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                                exit(1);
                        }
                        se->type = ELEMENT_TYPE_BOX;
                        se->element.box = box;

                        add_element_to_slide(new_slide, se);
                }
                if (slide->elements[i]->type == ELEMENT_TYPE_SLIDE) {
                        Slide* found_slide = copy_slide(slide->elements[i]->element.slide);
                        SlideElement* se = malloc(sizeof(SlideElement));
                        if (!se) {
                                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                                exit(1);
                        }
                        se->type = ELEMENT_TYPE_SLIDE;
                        se->element.slide = found_slide;

                        add_element_to_slide(new_slide, se);
                }
        }
        return new_slide;
}


Box* copy_box(Box* box)
{
        Box* new_box;
        unsigned int i;
        create_box(&new_box, strdup(box->name), box->stack_type, box->text_align);
        for (i = 0; i < box->element_count; i++) {
                if (box->elements[i]->type == ELEMENT_TYPE_TEXT) {
                        Text* text = copy_text(box->elements[i]->element.text);
                        SlideElement* se = malloc(sizeof(SlideElement));
                        if (!se) {
                                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                                exit(1);
                        }
                        se->type = ELEMENT_TYPE_TEXT;
                        se->element.text = text;

                        add_element_to_box(new_box, se);
                }
                if (box->elements[i]->type == ELEMENT_TYPE_IMAGE) {
                        Image* image = copy_image(box->elements[i]->element.image);
                        SlideElement* se = malloc(sizeof(SlideElement));
                        if (!se) {
                                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                                exit(1);
                        }
                        se->type = ELEMENT_TYPE_IMAGE;
                        se->element.image = image;

                        add_element_to_box(new_box, se);
                }
        }
        return new_box;
}


Text* copy_text(Text* text)
{
        Text* new_text;
        create_text(&new_text, strdup(text->content), text->font_size);
        return new_text;
}


Image* copy_image(Image* image)
{
        Image* new_image;
        create_image(&new_image, strdup(image->filename));
        return new_image;
}


Slide* find_slide_by_name(char* name, SlideList list)
{
        unsigned int i;
        for (i = 0; i < list.count; i++) {
                Slide* slide = list.slides[i];
                if (strcmp(slide->name, name) == 0) {
                        return slide;
                }
        }
        return NULL;
}


SlideElement* find_element_by_name(char* name, Slide* slide)
{
        unsigned int i;

        if (!slide)
                return NULL;
        if (slide->name && strcmp(slide->name, name) == 0) {
                fprintf(stderr, "Logic Error : Attempting to access %s inside of %s\n", name, slide->name);
                exit(1);
        }

        for (i = 0; i < slide->element_count; i++) {
                SlideElement* se = slide->elements[i];
                if (se->type == ELEMENT_TYPE_BOX && se->element.box->name && strcmp(se->element.box->name, name) == 0) {
                        return se;
                }
                else if (se->type == ELEMENT_TYPE_SLIDE && se->element.slide->name && strcmp(se->element.slide->name, name) == 0) {
                        return se;
                }
                else if (se->type == ELEMENT_TYPE_SLIDE) {
                        SlideElement* found_element = find_element_by_name(name, se->element.slide);
                        if (found_element != NULL) {
                                return found_element;
                        }
                }
        }
        return NULL;
}


void slide_list_init(SlideList *slide_list)
{
        slide_list->slides = NULL;
        slide_list->count = 0;
}


void slide_list_append(SlideList* slide_list, Slide* slide)
{
        slide_list->slides = realloc(slide_list->slides, (slide_list->count + 1) * sizeof(Slide*));
        if (!slide_list->slides) {
                fprintf(stderr, "Error reallocating memory for slide list\n");
                exit(1);
        }
        slide_list->slides[slide_list->count] = slide;
        slide_list->count++;
}


void slide_list_free(SlideList *slide_list)
{
        unsigned int i;
        for (i = 0; i < slide_list->count; i++) {
                free_slide(slide_list->slides[i]);
        }
        free(slide_list->slides);
}


void free_slide(Slide* slide)
{
        unsigned int element_index;

        free(slide->name);

        for (element_index = 0; element_index < slide->element_count; element_index++) {
                SlideElement* element = slide->elements[element_index];
                if (element->type == ELEMENT_TYPE_BOX) {
                        free_box(element->element.box);
                }
                if (element->type == ELEMENT_TYPE_SLIDE) {
                        free_slide(element->element.slide);
                }
                free(element);
        }
        free(slide->elements);
        free(slide);
}


void free_box(Box* box)
{
        unsigned int element_index;

        free(box->name);
        for (element_index = 0; element_index < box->element_count; element_index++) {
                SlideElement* element = box->elements[element_index];
                if (element->type == ELEMENT_TYPE_TEXT) {
                        free_text(element->element.text);
                }
                if (element->type == ELEMENT_TYPE_IMAGE) {
                        free_image(element->element.image);
                }
                if (element->type == ELEMENT_TYPE_BOX) {
                        free_box(element->element.box);
                }
                free(element);
        }
        free(box->elements);
        free(box);
}


void free_text(Text* text)
{
        free(text->content);
        text->content = NULL;
        free(text);
        text = NULL;
}


void free_image(Image* image)
{
        free(image->filename);
        image->ximage->data = NULL;
        XDestroyImage(image->ximage);
        stbi_image_free(image->data);
        free(image);
}


void add_element_to_box(Box* box, SlideElement* element)
{
        if (box->element_count == 0) {
                box->elements = malloc(sizeof(SlideElement *));
        }
        else {
                box->elements = realloc(box->elements, (box->element_count + 1) * sizeof(SlideElement *));
        }

        if (box->elements == NULL) {
                fprintf(stderr, "Error: Failed to allocate memory for box elements.\n");
                exit(1);
        }

        box->elements[box->element_count] = element;
        box->element_count++;
}


void add_element_to_box_at_index(Box* box, SlideElement* element, unsigned int index)
{
        box->elements = realloc(box->elements, (box->element_count + 1) * sizeof(SlideElement *));
        if (box->elements == NULL) {
                fprintf(stderr, "Error: Failed to allocate memory for box element.\n");
                exit(1);
        }
        memmove(&box->elements[index + 1], &box->elements[index], (box->element_count - index) * sizeof(SlideElement*));
        box->elements[index] = element;
        box->element_count ++;
}


void add_element_to_slide(Slide* slide, SlideElement* element)
{
        if (slide->element_count == 0) {
                slide->elements = malloc(sizeof(SlideElement *));
        }
        else {
                slide->elements = realloc(slide->elements, (slide->element_count + 1) * sizeof(SlideElement *));
        }

        if (slide->elements == NULL) {
                fprintf(stderr, "Error: Failed to allocate memory for slide elements.\n");
                exit(1);
        }

        slide->elements[slide->element_count] = element;
        slide->element_count++;
}


void check_syntax(char** check_args, unsigned int check_len, char* expect, unsigned int line_num)
{
        unsigned int arg_len;
        unsigned int i;
        char** types = split_str(expect, " ", &arg_len);

        if (check_len - 1 != arg_len) {
                fprintf(stderr, "Syntax Error on line %d : %s expects %d arguments, but %d were given.\n", line_num, check_args[0], arg_len, check_len - 1);
                exit(1);
        }

        for (i = 0; i < arg_len; i++) {
                unsigned int cur = i + 1;
                if (strcmp(types[i], "str") == 0) {
                        if (!is_string(check_args[cur])) {
                                fprintf(stderr, "Syntax Error on line %d : Expected String for argument %d", line_num, i);
                                exit(1);
                        }
                }
                else if (strcmp(types[i], "int") == 0) {
                        bool dummy;
                        if (!is_number(check_args[cur], &dummy)) {
                                fprintf(stderr, "Syntax Error on line %d : Expected Integer for argument %d", line_num, i);
                                exit(1);
                        }
                }
                else if (strcmp(types[i], "uint") == 0) {
                        bool is_negative = false;
                        if (!is_number(check_args[cur], &is_negative) || is_negative) {
                                fprintf(stderr, "Syntax Error on line %d : Expected Positive Integer for argument %d", line_num, i);
                                exit(1);
                        }
                }
                else if (strcmp(types[i], "type") == 0) {
                        bool dummy;
                        if (is_number(check_args[cur], &dummy) || is_string(check_args[cur])) {
                                fprintf(stderr, "Syntax Error on line %d : Expected variable for argument %d", line_num, i);
                                exit(1);
                        }
                }
        }

        free_split_str(types, arg_len);
}


bool is_number(char *str, bool* is_negative)
{
        char* endptr;
        long num;

        if (str == NULL || *str == '\0' || isspace(*str))
                return false;

        num = strtol(str, &endptr, 10);

        *is_negative = (num < 0);

        return (*endptr == '\0');
}


bool is_string(char* str)
{
        int len = strlen(str);
        if (len < 2)
                return false;
        if (str[0] != '"' && str[len - 1] != '"')
                return false;
        return true;
}


char* remove_quotes(const char* str)
{
        int length = strlen(str);
        char* new_str = (char*)malloc(sizeof(char) * (length - 1));

        if (new_str == NULL) {
                return NULL;
        }

        strncpy(new_str, str + 1, length - 2);
        new_str[length - 2] = '\0';

        return new_str;
}


void swap_rb_channels(unsigned char* img_data, int width, int height)
{
        int i;
        for (i = 0; i < width * height * 4; i += 4) {
                unsigned char temp = img_data[i];
                img_data[i] = img_data[i + 2];
                img_data[i + 2] = temp;
        }
}


void create_text(Text** text, char* content, FontSize font_size)
{
        (*text) = malloc(sizeof(Text));
        if (!*text) {
                fprintf(stderr, "Error: Failed to allocate memory for text\n");
                exit(1);
        }

        (*text)->type = ELEMENT_TYPE_TEXT;
        (*text)->content = content;
        (*text)->font_size = font_size;
}


void create_image(Image** image, char* filename)
{
        (*image) = malloc(sizeof(Image));
        if (!*image) {
                fprintf(stderr, "Error: Failed to allocate memory for text\n");
                exit(1);
        }

        (*image)->type = ELEMENT_TYPE_IMAGE;
        (*image)->filename = filename;
        (*image)->data = stbi_load(filename, &(*image)->width, &(*image)->height, &(*image)->channels, 4);
        swap_rb_channels((*image)->data, (*image)->width, (*image)->height);
        if ((*image)->data == NULL) {
                fprintf(stderr, "Failed to load image: %s\n", filename);
                exit(1);
        }
}


void create_box(Box** box, char* name, StackType stack, TextAlignmentType alignment)
{
        (*box) = malloc(sizeof(Box));
        if (!*box) {
                fprintf(stderr, "Error: Failed to allocate memory for box\n");
                exit(1);
        }

        (*box)->type = ELEMENT_TYPE_BOX;
        (*box)->name = name;
        (*box)->stack_type = stack;
        (*box)->text_align = alignment;

        (*box)->elements = NULL;
        (*box)->element_count = 0;
}


void create_slide(Slide** slide, char* name, bool visible)
{
        (*slide) = malloc(sizeof(Slide));
        if (!*slide) {
                fprintf(stderr, "Error: Failed to allocate memory for Slide\n");
                exit(1);
        }

        (*slide)->type = ELEMENT_TYPE_SLIDE;
        (*slide)->name = name;
        (*slide)->visible = visible;

        (*slide)->elements = NULL;
        (*slide)->element_count = 0;
}

/*
 * This function does a bit more than splitting a string by delimiter,
 * it doesn't split text enclosed in quotes, and also leading whitespace
 * is removed from each substring.
 */
char** split_str(char* str, const char* delim, unsigned int* length_return)
{
        char** split_str = NULL;
        bool in_quotes = false;
        bool in_token = false;
        int len = 0;
        int start_idx = 0;
        unsigned int i;

        for (i = 0; i <= strlen(str); i++) {
                if (str[i] == '\"') {
                        in_quotes = !in_quotes;
                        continue;
                }

                if (strchr(delim, str[i]) == NULL && str[i] != '\0') {
                        if (!in_token) {
                                in_token = true;
                                start_idx = i;
                        }
                }
                else if (in_token == true && (!in_quotes || str[i] == '\0')) {
                        in_token = 0;
                        len++;

                        split_str = realloc(split_str, len * sizeof(char*));
                        if (split_str == NULL) {
                                fprintf(stderr, "Error reallocating memory for split string.\n");
                                exit(1);
                        }

                        split_str[len - 1] = malloc(i - start_idx + 1);
                        if (split_str[len - 1] == NULL) {
                                fprintf(stderr, "Error allocating memory for split string.\n");
                                exit(1);
                        }

                        memcpy(split_str[len - 1], &str[start_idx], i - start_idx);
                        split_str[len - 1][i - start_idx] = '\0';
                        trim_str(split_str[len - 1]);
                }
        }
        *length_return = len;
        return split_str;
}


void free_split_str(char** split_str, unsigned int len)
{
        unsigned int i;
        for (i = 0; i < len; i++) {
                free(split_str[i]);
        }
        free(split_str);
}


void trim_str(char* str)
{
        char* start = str;
        char* end;
        while (isspace((unsigned char)*start))
                start++;

        memmove(str, start, strlen(start) + 1);
        end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end)) {
                end--;
        }
        *(end + 1) = '\0';
}

/* Handler functions */

void handle_slide(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str";
        (void) current_box;
        (void) list;
        check_syntax(args, argc, expected, line_num);

        create_slide(current_slide, remove_quotes(args[1]), true);
}


void handle_template(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str";
        (void) current_box;
        (void) list;
        check_syntax(args, argc, expected, line_num);

        create_slide(current_slide, remove_quotes(args[1]), false);
}


void handle_box(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str type type";
        Box* box = NULL;
        SlideElement* se = NULL;
        StackType stack_type;
        TextAlignmentType text_align;
        (void) current_box;
        (void) list;

        check_syntax(args, argc, expected, line_num);

        if (strcmp(args[2], "stack-vertical") == 0) {
                stack_type = STACK_VERTICAL;
        }
        else if (strcmp(args[2], "stack-horizontal") == 0) {
                stack_type = STACK_HORIZONTAL;
        }
        else {
                fprintf(stderr, "Syntax Error on line %d : Expected 'stack-horizontal' or 'stack-vertical' for argument 2 but found %s\n", line_num, args[2]);
        }

        if (strcmp(args[3], "align-left") == 0) {
                text_align = TEXT_ALIGN_LEFT;
        }
        else if (strcmp(args[3], "align-right") == 0) {
                text_align = TEXT_ALIGN_RIGHT;
        }
        else if (strcmp(args[3], "align-center") == 0) {
                text_align = TEXT_ALIGN_CENTER;
        }
        else {
                fprintf(stderr, "Syntax Error on line %d : Expected 'align-left', 'align-right', or 'align-center' for argument 3 but found %s\n", line_num, args[3]);
        }

        create_box(&box, remove_quotes(args[1]), stack_type, text_align);

        se = malloc(sizeof(SlideElement));
        if (!se) {
                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                exit(1);
        }
        se->type = ELEMENT_TYPE_BOX;
        se->element.box = box;

        add_element_to_slide(*current_slide, se);
}


void handle_uses(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str";
        Slide* slide;
        Slide* found_slide;
        SlideElement* se;
        char* name;
        (void) current_box;
        check_syntax(args, argc, expected, line_num);
        name = remove_quotes(args[1]);

        found_slide = find_slide_by_name(name, list);
        if (found_slide == NULL) {
                fprintf(stderr, "Error on line %d : Couldn't find slide or template with name: %s\n", line_num, name);
                exit(1);
        }
        slide = copy_slide(found_slide);

        se = malloc(sizeof(SlideElement));
        if (!se) {
                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                exit(1);
        }
        se->type = ELEMENT_TYPE_SLIDE;
        se->element.slide = slide;

        add_element_to_slide(*current_slide, se);

        free(name);
}


void handle_text(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "type str";
        SlideElement* se = NULL;
        Text* text = NULL;
        FontSize font_size;
        (void) current_slide;
        (void) list;
        check_syntax(args, argc, expected, line_num);

        if (strcmp(args[1], "huge") == 0) {
                font_size = FONT_HUGE;
        }
        else if (strcmp(args[1], "title") == 0) {
                font_size = FONT_TITLE;
        }
        else if (strcmp(args[1], "normal") == 0) {
                font_size = FONT_NORMAL;
        }
        else if (strcmp(args[1], "small") == 0) {
                font_size = FONT_SMALL;
        }
        else {
                fprintf(stderr, "Syntax Error on line %d : Expected 'title', 'normal', or 'small' for argument 1 but found %s\n", line_num, args[1]);
        }

        create_text(&text, remove_quotes(args[2]), font_size);

        se = malloc(sizeof(SlideElement));
        if (!se) {
                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                exit(1);
        }
        se->type = ELEMENT_TYPE_TEXT;
        se->element.text = text;

        if (*current_box == NULL) {
                fprintf(stderr, "Logic Error on line %d : Attempting to add text to non-box object.\n", line_num);
        }
        add_element_to_box(*current_box, se);
}


void handle_image(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str";
        SlideElement* se = NULL;
        Image* image = NULL;
        (void) current_slide;
        (void) list;
        check_syntax(args, argc, expected, line_num);
        create_image(&image, remove_quotes(args[1]));

        se = malloc(sizeof(SlideElement));
        if (!se) {
                fprintf(stderr, "Error: Failed to allocate memory for SlideElement.\n");
                exit(1);
        }
        se->type = ELEMENT_TYPE_IMAGE;
        se->element.image = image;

        if (*current_box == NULL) {
                fprintf(stderr, "Logic Error on line %d : Attempting to add text to non-box object.\n", line_num);
        }
        add_element_to_box(*current_box, se);
}


void handle_define(SlideList list, Slide** current_slide, Box** current_box, char** args, unsigned int argc, unsigned int line_num)
{
        char expected[] = "str";
        char* name = remove_quotes(args[1]);
        SlideElement* se;
        (void) current_box;
        (void) list;

        check_syntax(args, argc, expected, line_num);
        se = find_element_by_name(name, *current_slide);
        free(name);

        if (se == NULL) {
                fprintf(stderr, "Logic Error on line %d : Trying to define nonexistent element '%s'.\n", line_num, args[1]);
                exit(1);
        }
        if (se->type != ELEMENT_TYPE_BOX) {
                fprintf(stderr, "Logic Error on line %d : Trying to define non-box element '%s'.\n", line_num, args[1]);
                exit(1);
        }

        *current_box = se->element.box;
}
