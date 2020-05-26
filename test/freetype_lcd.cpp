#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#include <shellscalingapi.h>
#endif

#include "agg_basics.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_u.h"
#include "agg_scanline_bin.h"
#include "agg_renderer_scanline.h"
#include "agg_renderer_primitives.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_conv_curve.h"
#include "agg_conv_contour.h"
#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgb24_lcd.h"
#include "agg_font_freetype.h"
#include "platform/agg_platform_support.h"
#include "agg_gamma_lut.h"

#include "ctrl/agg_slider_ctrl.h"
#include "ctrl/agg_cbox_ctrl.h"
#include "ctrl/agg_rbox_ctrl.h"


enum { flip_y = true };


static const char *os_font_directory;

struct fontname_pair {
    const char *regular;
    const char *italic;
};

struct color_scheme {
    agg::int8u foreground[3];
    agg::int8u background[3];
};

const color_scheme predefined_colors[] = {
    { {0x00, 0x00, 0x00}, {0xff, 0xff, 0xff} },
    { {0x23, 0x1f, 0x20}, {0xe9, 0xe5, 0xcd} },
    { {0x38, 0x9a, 0xd4}, {0xe9, 0xe5, 0xcd} },
    { {0x23, 0x1f, 0x20}, {0x7c, 0xb1, 0xe0} },
    { {0xff, 0xff, 0xff}, {0x7c, 0xb1, 0xe0} },
};

static agg::rgba8 rgba8_of_values(const agg::int8u value[]) {
    return agg::rgba8(value[0], value[1], value[2]);
}

#if defined(_WIN32) || defined(WIN32)
static const fontname_pair os_fontnames[5] = {
    {"arial.ttf", "ariali.ttf"},
    {"tahoma.ttf", "tahomai.ttf"},
    {"verdana.ttf", "verdanai.ttf"},
    {"times.ttf", "timesi.ttf"},
    {"georgia.ttf", "georgiai.ttf"},
};
static const char *os_face_name[5] = {"Arial", "Tahoma", "Verdana", "Times", "Georgia"};
static const char *os_default_font_dir = "C:/Windows/fonts";
#else
static const fontname_pair os_fontnames[5] = {
    {"freefont/FreeSans.ttf", "freefont/FreeSansOblique.ttf"},
    {"fonts-deva-extra/kalimati.ttf", "fonts-deva-extra/kalimati.ttf"},
    {"dejavu/DejaVuSans.ttf", "dejavu/DejaVuSans.ttf"},
    {"freefont/FreeSerif.ttf", "freefont/FreeSerifItalic.ttf"},
    {"liberation/LiberationSerif-Regular.ttf", "liberation/LiberationSerif-Italic.ttf"},
};
static const char *os_face_name[5] = {"FreeSans", "Kalimati", "DejaVuSans", "FreeSerif", "LiberationSerif"};
static const char *os_default_font_dir = "/usr/share/fonts/truetype";
#endif

static char text1[] = 
"A single pixel on a color LCD is made of three colored elements \n"
"ordered (on various displays) either as blue, green, and red (BGR), \n"
"or as red, green, and blue (RGB). These pixel components, sometimes \n"
"called sub-pixels, appear as a single color to the human eye because \n"
"of blurring by the optics and spatial integration by nerve cells in the eye.";

static char text2[] = 
"The components are easily visible, however, when viewed with \n"
"a small magnifying glass, such as a loupe. Over a certain resolution \n"
"range the colors in the sub-pixels are not visible, but the relative \n"
"intensity of the components shifts the apparent position or orientation \n"
"of a line. Methods that take this interaction between the display \n"
"technology and the human visual system into account are called \n"
"subpixel rendering algorithms.";

static char text3[] = 
"The resolution at which colored sub-pixels go unnoticed differs, \n"
"however, with each user some users are distracted by the colored \n"
"\"fringes\" resulting from sub-pixel rendering. Subpixel rendering \n"
"is better suited to some display technologies than others. The \n"
"technology is well-suited to LCDs, but less so for CRTs. In a CRT \n"
"the light from the pixel components often spread across pixels, \n"
"and the outputs of adjacent pixels are not perfectly independent.";


static char text4[] = 
"If a designer knew precisely a great deal about the display's \n"
"electron beams and aperture grille, subpixel rendering might \n"
"have some advantage. But the properties of the CRT components, \n"
"coupled with the alignment variations that are part of the \n"
"production process, make subpixel rendering less effective for \n"
"these displays. The technique should have good application to \n"
"organic light emitting diodes and other display technologies.";




template<class VertexSource> class faux_weight
{
public:
    faux_weight(VertexSource& vs) : 
        m_mtx_zoom_in_y(agg::trans_affine_scaling(1, 100)),
        m_mtx_zoom_out_y(agg::trans_affine_scaling(1, 1.0/100.0)),
        m_source(&vs),
        m_trans_zoom_in_y(*m_source, m_mtx_zoom_in_y),
        m_contour(m_trans_zoom_in_y),
        m_trans_zoom_out(m_contour, m_mtx_zoom_out_y) 
    {
        m_contour.auto_detect_orientation(false);
    }

    void weight(double v) { m_contour.width(v); }

    void rewind(unsigned path_id=0) {m_trans_zoom_out.rewind(path_id); }
    unsigned vertex(double* x, double* y) { return m_trans_zoom_out.vertex(x, y); }

private:
    agg::trans_affine m_mtx_zoom_in_y;
    agg::trans_affine m_mtx_zoom_out_y;
    VertexSource* m_source;
    agg::conv_transform<VertexSource> m_trans_zoom_in_y;
    agg::conv_contour<agg::conv_transform<VertexSource> > m_contour;
    agg::conv_transform<agg::conv_contour<agg::conv_transform<VertexSource> > > m_trans_zoom_out;
};



class the_application : public agg::platform_support
{
    typedef agg::pixfmt_rgb24 pixfmt_type;
    typedef agg::renderer_base<pixfmt_type> base_ren_type;
    typedef agg::renderer_scanline_aa_solid<base_ren_type> renderer_solid;
    typedef agg::font_engine_freetype_int32 font_engine_type;
    typedef agg::font_cache_manager<font_engine_type> font_manager_type;

    agg::rbox_ctrl<agg::rgba8>   m_typeface;
    agg::rbox_ctrl<agg::rgba8>   m_color_scheme;
    agg::slider_ctrl<agg::rgba8> m_height;
    agg::slider_ctrl<agg::rgba8> m_faux_italic;
    agg::slider_ctrl<agg::rgba8> m_faux_weight;
    agg::slider_ctrl<agg::rgba8> m_interval;
    agg::slider_ctrl<agg::rgba8> m_width;

    agg::slider_ctrl<agg::rgba8> m_gamma;
    agg::slider_ctrl<agg::rgba8> m_primary;
    agg::cbox_ctrl<agg::rgba8>   m_grayscale;
    agg::cbox_ctrl<agg::rgba8>   m_hinting;
    agg::cbox_ctrl<agg::rgba8>   m_kerning;
    agg::cbox_ctrl<agg::rgba8>   m_invert;
    font_engine_type             m_feng;
    font_manager_type            m_fman;
    double                       m_old_height;
    agg::gamma_lut<>             m_gamma_lut;

    // Pipeline to process the vectors glyph paths (curves + contour)
    agg::trans_affine m_mtx;
    agg::conv_curve<font_manager_type::path_adaptor_type> m_curves;
    agg::conv_transform<agg::conv_curve<font_manager_type::path_adaptor_type> > m_trans;
    faux_weight<agg::conv_transform<agg::conv_curve<font_manager_type::path_adaptor_type> > > m_weight;
public:
    the_application(agg::pix_format_e format, bool flip_y) :
        agg::platform_support(format, flip_y),
        m_typeface     (5.0, 5.0, 5.0+130.0,   110.0,  !flip_y),
        m_color_scheme (140.0, 5.0, 5.0+190.0,   110.0,  !flip_y),

        m_height       (200, 10.0, 640-5.0,    17.0,   !flip_y),
        m_faux_italic  (200, 25.0, 640-5.0,    32.0,   !flip_y),
        m_faux_weight  (200, 40.0, 640-5.0,    47.0,   !flip_y),
        m_interval     (300, 55.0, 640-5.0,    62.0,   !flip_y),
        m_width        (300, 70.0, 640-5.0,    77.0,   !flip_y),
        m_gamma        (300, 85.0, 640-5.0,    92.0,   !flip_y),
        m_primary      (300,100.0, 640-5.0,   107.0,   !flip_y),

        m_grayscale    (200, 50.0, "Grayscale", !flip_y),
        m_hinting      (200, 65.0, "Hinting",   !flip_y),
        m_kerning      (200, 80.0, "Kerning",   !flip_y),
        m_invert       (200, 95.0, "Invert",    !flip_y),
        m_feng(),
        m_fman(m_feng),
        m_old_height(0.0),
        m_curves(m_fman.path_adaptor()),
        m_trans(m_curves, m_mtx),
        m_weight(m_trans)
    {
        for (int i = 0; i < 5; i++) {
            m_typeface.add_item(os_face_name[i]);
        }
        m_typeface.cur_item(4);
        add_ctrl(m_typeface);
        m_typeface.no_transform();

        static const char *scheme_name[] = {"1", "2", "3", "4", "5"};
        for (int i = 0; i < 5; i++) {
            m_color_scheme.add_item(scheme_name[i]);
        }
        m_color_scheme.cur_item(1);
        add_ctrl(m_color_scheme);
        m_color_scheme.no_transform();

        m_height.label("Font Scale=%.2f");
        m_height.range(0.5, 2);
        m_height.value(1);
        add_ctrl(m_height);
        m_height.no_transform();

        m_faux_weight.label("Faux Weight=%.2f");
        m_faux_weight.range(-1, 1);
        m_faux_weight.value(0);
        add_ctrl(m_faux_weight);
        m_faux_weight.no_transform();

        m_faux_italic.label("Faux Italic=%.2f");
        m_faux_italic.range(-1, 1);
        m_faux_italic.value(0);
        add_ctrl(m_faux_italic);
        m_faux_italic.no_transform();

        m_interval.label("Interval=%.3f");
        m_interval.range(-0.2, 0.2);
        m_interval.value(0);
        add_ctrl(m_interval);
        m_interval.no_transform();

        m_width.label("Width=%.2f");
        m_width.range(0.75, 1.25);
        m_width.value(1);
        add_ctrl(m_width);
        m_width.no_transform();

        m_gamma.label("Gamma=%.2f");
        m_gamma.range(0.5, 2.5);
        m_gamma.value(1.8);
        add_ctrl(m_gamma);
        m_gamma.no_transform();

        m_primary.label("Primary Weight=%.2f");
        m_primary.range(0.0, 1.0);
        m_primary.value(0.448); // Standard, non-tuned value is 1./3.
        add_ctrl(m_primary);
        m_primary.no_transform();

        add_ctrl(m_hinting);
        m_hinting.status(true);
        m_hinting.no_transform();

        add_ctrl(m_kerning);
        m_kerning.status(true);
        m_kerning.no_transform();

        add_ctrl(m_invert);
        m_invert.no_transform();

        add_ctrl(m_grayscale);
        m_grayscale.no_transform();
    }



    template<class Rasterizer, class Scanline, class RenSolid>
    double draw_text(Rasterizer& ras, Scanline& sl, 
                     RenSolid& ren_solid, const color_scheme& scheme,
                     const char* font, const char* text,
                     double x, double y, double height,
                     unsigned subpixel_scale)
    {
        agg::rgba8 color = rgba8_of_values(scheme.foreground);
        if(m_invert.status())
        {
            color = rgba8_of_values(scheme.background);
        }

        agg::glyph_rendering gren = agg::glyph_ren_outline;

        double scale_x = 100;

        if(m_feng.load_font(full_file_name(font), 0, gren))
        {
            m_feng.height(height);
            m_feng.width(height * scale_x * subpixel_scale);
            m_feng.hinting(m_hinting.status());

            const char* p = text;

            x *= subpixel_scale;
            double start_x = x;

            while(*p)
            {
                if(*p == '\n')
                {
                    x = start_x;
                    y -= height * 1.25;
                    ++p;
                    continue;
                }

                const agg::glyph_cache* glyph = m_fman.glyph(*p);
                if(glyph)
                {
                    if(m_kerning.status())
                    {
                        m_fman.add_kerning(&x, &y);
                    }

                    m_fman.init_embedded_adaptors(glyph, 0, 0);
                    if(glyph->data_type == agg::glyph_data_outline)
                    {
                        double ty = m_hinting.status() ? floor(y + 0.5) : y;
                        ras.reset();
                        m_mtx.reset();
                        m_mtx *= agg::trans_affine_scaling(m_width.value() / scale_x, 1);
                        m_mtx *= agg::trans_affine_skewing(m_faux_italic.value() * subpixel_scale / 3, 0);
                        m_mtx *= agg::trans_affine_translation(start_x + x/scale_x, ty);
                        if(fabs(m_faux_weight.value()) < 0.05)
                        {
                            ras.add_path(m_trans);
                        }
                        else
                        {
                            m_weight.weight(-m_faux_weight.value() * height * subpixel_scale / 15);
                            ras.add_path(m_weight);
                        }
                        ren_solid.color(color);
                        agg::render_scanlines(ras, sl, ren_solid);
                    }

                    // increment pen position
                    x += glyph->advance_x + m_interval.value() * scale_x * subpixel_scale;
                    y += glyph->advance_y;
                }
                ++p;
            }
        }
        return y;
    }


    static char *path_join(const char *dir, const char *filename) {
        const int path_len = strlen(dir), filename_len = strlen(filename);
        const int len = path_len + 1 + filename_len + 1;
        char *full_name = new char[len];
        memcpy(full_name, dir, path_len);
        memcpy(full_name + path_len, "/", 1);
        memcpy(full_name + path_len + 1, filename, filename_len);
        full_name[len - 1] = 0;
        return full_name;
    }

    virtual void on_draw()
    {
        m_gamma_lut.gamma(m_gamma.value());

        agg::pixfmt_rgb24 pf(rbuf_window());
        base_ren_type ren_base(pf);
        renderer_solid ren_solid(ren_base);

        int color_index = m_color_scheme.cur_item();
        const color_scheme& color_pair = predefined_colors[color_index];

        ren_base.clear(rgba8_of_values(color_pair.background));

        if(m_invert.status())
        {
            ren_base.copy_bar(0, 120, pf.width(), pf.height(), rgba8_of_values(color_pair.foreground));
        }


        agg::scanline_u8 sl;
        agg::rasterizer_scanline_aa<> ras;
        ras.clip_box(0, 120, pf.width()*3, pf.height());

        // Conventional LUT values: (1./3., 2./9., 1./9.)
        // The values below are fine tuned as in the Elementary Plot library.
        // The primary weight should be 0.448 but is left adjustable.
        agg::lcd_distribution_lut lut(m_primary.value(), 0.184, 0.092);
        typedef agg::pixfmt_rgb24_lcd_gamma<agg::gamma_lut<> > pixfmt_lcd_type;
        pixfmt_lcd_type pf_lcd(rbuf_window(), lut, m_gamma_lut);
        agg::renderer_base<pixfmt_lcd_type> ren_base_lcd(pf_lcd);
        agg::renderer_scanline_aa_solid<agg::renderer_base<pixfmt_lcd_type> > ren_solid_lcd(ren_base_lcd);


        double y = height() - 20;
        double k = m_height.value();

        static fontname_pair *fontnames_full_path = 0;
        if (fontnames_full_path == 0) {
            fontnames_full_path = new fontname_pair[5];
            for (int i = 0; i < 5; i++) {
                const fontname_pair *pair = &os_fontnames[i];
                fontnames_full_path[i].regular = path_join(os_font_directory, pair->regular);
                fontnames_full_path[i].italic  = path_join(os_font_directory, pair->italic);
            }
        }

        int typeface_index = m_typeface.cur_item();
        const char* normal = fontnames_full_path[typeface_index % 5].regular;
        const char* italic = fontnames_full_path[typeface_index % 5].italic;

        //--------------------------------------
        if(m_grayscale.status())
        {
            y  = draw_text(ras, sl, ren_solid, color_pair, normal, text1, 10, y, k*12, 1); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid, color_pair, italic, text2, 10, y, k*12, 1); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid, color_pair, normal, text3, 10, y, k*12, 1); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid, color_pair, italic, text4, 10, y, k*12, 1); 
            y -= 7 + k*12;
        }
        else
        {
            y  = draw_text(ras, sl, ren_solid_lcd, color_pair, normal, text1, 10, y, k*12, 3); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid_lcd, color_pair, italic, text2, 10, y, k*12, 3); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid_lcd, color_pair, normal, text3, 10, y, k*12, 3); 
            y -= 7 + k*12;
            y  = draw_text(ras, sl, ren_solid_lcd, color_pair, italic, text4, 10, y, k*12, 3); 
            y -= 7 + k*12;
        }


/*
        //--------------------------------------
        if(m_grayscale.status())
        {
            for(int i = 0; i < 30; ++i)
            {
                draw_text(ras, sl, ren_solid, normal, pangram, 10+i/10., y, k*11, 1);  y -= k*11;
            }
        }
        else
        {
            for(int i = 0; i < 30; ++i)
            {
                draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10+i/10., y, k*11, 3);  y -= k*11;
            }
        }
*/


/*
        //--------------------------------------
        if(m_grayscale.status())
        {
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*9,  1); y -= 5+k*9;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*10, 1); y -= 5+k*10;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*11, 1); y -= 5+k*11;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*12, 1); y -= 5+k*12;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*13, 1); y -= 5+k*13;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*14, 1); y -= 5+k*14;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*15, 1); y -= 5+k*15;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*16, 1); y -= 5+k*16;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*17, 1); y -= 5+k*17;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*18, 1); y -= 5+k*18;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*19, 1); y -= 5+k*19;
            draw_text(ras, sl, ren_solid, normal, pangram, 10, y, k*20, 1); y -= 5+k*20;
        }
        else
        {
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*9, 3);  y -= 5+k*9;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*10, 3); y -= 5+k*10;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*11, 3); y -= 5+k*11;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*12, 3); y -= 5+k*12;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*13, 3); y -= 5+k*13;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*14, 3); y -= 5+k*14;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*15, 3); y -= 5+k*15;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*16, 3); y -= 5+k*16;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*17, 3); y -= 5+k*17;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*18, 3); y -= 5+k*18;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*19, 3); y -= 5+k*19;
            draw_text(ras, sl, ren_solid_lcd, normal, pangram, 10, y, k*20, 3); y -= 5+k*20;
        }
*/


        ras.clip_box(0, 0, pf.width(), pf.height());

        agg::render_ctrl(ras, sl, ren_base, m_color_scheme);
        agg::render_ctrl(ras, sl, ren_base, m_typeface);
        agg::render_ctrl(ras, sl, ren_base, m_height);
        agg::render_ctrl(ras, sl, ren_base, m_faux_italic);
        agg::render_ctrl(ras, sl, ren_base, m_faux_weight);
        agg::render_ctrl(ras, sl, ren_base, m_interval);
        agg::render_ctrl(ras, sl, ren_base, m_width);
        agg::render_ctrl(ras, sl, ren_base, m_gamma);
        agg::render_ctrl(ras, sl, ren_base, m_primary);
        agg::render_ctrl(ras, sl, ren_base, m_hinting);
        agg::render_ctrl(ras, sl, ren_base, m_kerning);
        agg::render_ctrl(ras, sl, ren_base, m_invert);
        agg::render_ctrl(ras, sl, ren_base, m_grayscale);

    }



    virtual void on_ctrl_change()
    {
    }

};



int agg_main(int argc, char* argv[])
{
    if (argc > 2) {
        fprintf(stderr, "usage: %s [truetype-font-dir]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        os_font_directory = argv[1];
    } else {
        os_font_directory = os_default_font_dir;
        fprintf(stderr, "Looking for truetype fonts in %s.\n", os_font_directory);
        fprintf(stderr,
            "If it does not work you may call the demo providing\n" \
            "the directory containing the truetype fonts as an argument.\n");
    }

#if defined(_WIN32) || defined(WIN32)
    SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
#endif

    the_application app(agg::pix_format_rgb24, flip_y);
    app.caption("AGG Example. A New Way of Text Rasterization");

    if(app.init(640, 560, agg::window_resize))
    {
        return app.run();
    }
    return 1;
}
