/*
 * Copyright (c) 2018-2026 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <bdk.h>

#include "gui.h"
#include "gui_info.h"
#include "../config.h"
#include <libs/lvgl/lv_themes/lv_theme_hekate.h>
#include <libs/lvgl/lvgl.h>
//====================================
//  ASAP: NAND config lock pin math.
//====================================
#include <libs/lvgl/lv_misc/lv_math.h>
//====================================

#define CLOCK_MIN_YEAR 2026
#define CLOCK_MAX_YEAR (CLOCK_MIN_YEAR + 10)
#define CLOCK_YEARLIST "2026년\n2027년\n2028년\n2029년\n2030년\n2031년\n2032년\n2033년\n2034년\n2035년\n2036년"

static lv_obj_t *autoboot_btn;

static bool ini_changes_made = false;
static bool nyx_changes_made = false;

static s32 timeoffset_backup = 0;
static bool timeoffset_backup_valid = false;

void nyx_options_clear_ini_changes_made()
{
	ini_changes_made = false;
}

//==========================
//  ASAP: Direct nyx save.
//==========================
static lv_res_t _save_options_action(lv_obj_t *btn)
{
	if (sd_mount()) {
		create_config_entry();
	}
	nyx_options_clear_ini_changes_made();
	sd_unmount();

	return LV_RES_OK;
}
//==========================

/* bool nyx_options_get_ini_changes_made()
{
	return ini_changes_made;
}

static lv_res_t auto_hos_poweroff_toggle(lv_obj_t *btn)
{
	h_cfg.autohosoff = !h_cfg.autohosoff;
	ini_changes_made = true;

	if (!h_cfg.autohosoff)
		lv_btn_set_state(btn, LV_BTN_STATE_REL);
	else
		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);

	nyx_generic_onoff_toggle(btn);

	return LV_RES_OK;
}

static lv_res_t auto_nogc_toggle(lv_obj_t *btn)
{
	h_cfg.autonogc = !h_cfg.autonogc;
	ini_changes_made = true;

	if (!h_cfg.autonogc)
		lv_btn_set_state(btn, LV_BTN_STATE_REL);
	else
		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);

	nyx_generic_onoff_toggle(btn);

	return LV_RES_OK;
}

static lv_res_t _update_r2p_action(lv_obj_t *btn)
{
	h_cfg.updater2p = !h_cfg.updater2p;
	ini_changes_made = true;

	if (!h_cfg.updater2p)
		lv_btn_set_state(btn, LV_BTN_STATE_REL);
	else
		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);

	nyx_generic_onoff_toggle(btn);

	return LV_RES_OK;
} */

static lv_res_t _win_autoboot_close_action(lv_obj_t * btn)
{
	if (!h_cfg.autoboot)
		lv_btn_set_state(autoboot_btn, LV_BTN_STATE_REL);
	else
		lv_btn_set_state(autoboot_btn, LV_BTN_STATE_TGL_REL);

	nyx_generic_onoff_toggle(autoboot_btn);

	lv_obj_t * win = lv_win_get_from_btn(btn);

	lv_obj_del(win);

	close_btn = NULL;

	return LV_RES_INV;
}

lv_obj_t *create_window_autoboot(const char *win_title)
{
	static lv_style_t win_bg_style;

	lv_style_copy(&win_bg_style, &lv_style_plain);
	win_bg_style.body.main_color = LV_COLOR_HEX(theme_bg_color);
	win_bg_style.body.grad_color = win_bg_style.body.main_color;

	lv_obj_t *win = lv_win_create(lv_scr_act(), NULL);
	lv_win_set_title(win, win_title);
	lv_win_set_style(win, LV_WIN_STYLE_BG, &win_bg_style);
	lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);

	close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE" 닫기", _win_autoboot_close_action);

	return win;
}

//========================================
//  ASAP: Autoboot dropdown list config.
//========================================
lv_res_t _autoboot_list_action(lv_obj_t *ddlist)
{
	u32 new_selection = lv_ddlist_get_selected(ddlist);
	if (new_selection == 0)
	{
		h_cfg.autoboot = 0;
		h_cfg.autoboot_list = 0;
	} else {
		h_cfg.autoboot = new_selection;
		h_cfg.autoboot_list = 1;
	}
	ini_changes_made = true;
	_save_options_action(ddlist);

	return LV_RES_OK;
}

//===========================================
//  ASAP: Autoboot launching delay seconds.
//===========================================
lv_res_t _autoboot_delay_action(lv_obj_t *ddlist)
{
	u32 new_selection = lv_ddlist_get_selected(ddlist);
	if (h_cfg.bootwait != new_selection)
	{
		h_cfg.bootwait = new_selection;
		ini_changes_made = true;
		_save_options_action(ddlist);
	}

	return LV_RES_OK;
}

static lv_res_t _save_nyx_options_action(lv_obj_t *btn)
{
	create_nyx_config_entry(true);
	nyx_changes_made = false;

	return LV_RES_OK;
}

static lv_res_t _slider_brightness_action(lv_obj_t * slider)
{
	u32 new_value = lv_slider_get_value(slider);

	if (h_cfg.backlight != new_value)
	{
		display_backlight_brightness(new_value - 20, 0);
		h_cfg.backlight = new_value;
		ini_changes_made = true;

		_save_options_action(slider);
	}

	return LV_RES_OK;
}

lv_res_t _data_verification_action(lv_obj_t *ddlist)
{
	u32 new_selection = lv_ddlist_get_selected(ddlist);
	if (n_cfg.verification != new_selection)
	{
		n_cfg.verification = new_selection;
		nyx_changes_made = true;
		_save_nyx_options_action(NULL);
	}

	return LV_RES_OK;
}

void create_flat_button(lv_obj_t *btn, int color_idx, lv_action_t action)
{
	lv_color_t color = color_idx ? lv_color_hsv_to_rgb(color_idx, 100, 100) : lv_color_hsv_to_rgb(53, 8, 90);
	lv_style_t *btn_onoff_rel_hos_style = malloc(sizeof(lv_style_t));
	lv_style_t *btn_onoff_pr_hos_style  = malloc(sizeof(lv_style_t));
	lv_style_copy(btn_onoff_rel_hos_style, lv_theme_get_current()->btn.rel);
	btn_onoff_rel_hos_style->body.main_color  = color;
	btn_onoff_rel_hos_style->body.grad_color  = btn_onoff_rel_hos_style->body.main_color;
	btn_onoff_rel_hos_style->body.padding.hor = 0;
	btn_onoff_rel_hos_style->body.radius      = 0;

	if (color_idx == 167)
	{
		btn_onoff_rel_hos_style->body.border.color = LV_COLOR_HEX(0x000000);
		btn_onoff_rel_hos_style->body.border.opa = LV_OPA_20;
		btn_onoff_rel_hos_style->body.border.width = 3;
	}

	lv_style_copy(btn_onoff_pr_hos_style, lv_theme_get_current()->btn.pr);
	btn_onoff_pr_hos_style->body.main_color   = color;
	btn_onoff_pr_hos_style->body.grad_color   = btn_onoff_pr_hos_style->body.main_color;
	btn_onoff_pr_hos_style->body.padding.hor  = 0;
	btn_onoff_pr_hos_style->body.border.color = LV_COLOR_HEX(0xFFFFFF);
	btn_onoff_pr_hos_style->body.border.opa   = LV_OPA_50;
	btn_onoff_pr_hos_style->body.border.width = 4;
	btn_onoff_pr_hos_style->body.radius       = 0;

	lv_btn_set_style(btn, LV_BTN_STYLE_REL,     btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_PR,      btn_onoff_pr_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_REL, btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_PR,  btn_onoff_pr_hos_style);

	lv_btn_set_fit(btn, false, false);
	lv_obj_set_size(btn, LV_DPI * 7 / 11, LV_DPI * 7 / 11);
	lv_btn_set_toggle(btn, true);

	if (action)
		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, action);
}

typedef struct _color_test_ctxt
{
	u32 bg;
	u16 hue;
	u8 r;
	u8 g;
	u8 b;

	lv_obj_t *window;
	lv_obj_t *header1;
	lv_obj_t *header2;
	lv_obj_t *label;
	lv_obj_t *icons;
	lv_obj_t *slider;
	lv_obj_t *button;
	lv_obj_t *hue_slider;
	lv_obj_t *hue_label;

	lv_obj_t *r_slider;
	lv_obj_t *r_label;
	lv_obj_t *g_slider;
	lv_obj_t *g_label;
	lv_obj_t *b_slider;
	lv_obj_t *b_label;

	lv_style_t box_style;
	lv_obj_t *box;

	lv_obj_t *btn_reset;
	lv_obj_t *btn_apply;
	lv_obj_t *btn_black;
} color_test_ctxt;

color_test_ctxt color_test;

static lv_res_t _action_win_nyx_colors_save(lv_obj_t *btn)
{
	n_cfg.theme_bg    = color_test.bg;
	n_cfg.theme_color = color_test.hue;

	// Save nyx config.
	create_nyx_config_entry(true);

	reload_nyx(NULL, false);

	return LV_RES_OK;
}

static void _show_new_nyx_color(bool update_bg)
{
	u32 bg = color_test.bg;
	u16 hue = color_test.hue;

	lv_color_t bgc       = LV_COLOR_HEX(bg);                              // COLOR_HOS_BG.
	lv_color_t bgc_light = LV_COLOR_HEX(bg ? (bg + 0x101010) : 0x2D2D2D); // COLOR_HOS_BG_LIGHT.
	lv_color_t bgc_press = LV_COLOR_HEX(bg ? (bg + 0x232323) : 0x404040); // 0x505050.
	lv_color_t bg_border = LV_COLOR_HEX(bg ? (bg + 0x202020) : 0x3D3D3D); // COLOR_HOS_BG_LIGHTER.
	lv_color_t color     = hue ? lv_color_hsv_to_rgb(hue, 100, 100) : lv_color_hsv_to_rgb(53, 8, 90);

	static lv_style_t btn_tgl_pr_test;
	lv_style_copy(&btn_tgl_pr_test, lv_btn_get_style(color_test.button, LV_BTN_STATE_TGL_PR));
	btn_tgl_pr_test.body.main_color = bgc_press;
	btn_tgl_pr_test.body.grad_color = btn_tgl_pr_test.body.main_color;
	btn_tgl_pr_test.body.border.color = color;
	btn_tgl_pr_test.text.color = color;

	if (update_bg)
	{
		static lv_style_t win_bg_test;
		lv_style_copy(&win_bg_test, lv_win_get_style(color_test.window, LV_WIN_STYLE_BG));
		win_bg_test.body.main_color = bgc;
		win_bg_test.body.grad_color = win_bg_test.body.main_color;
		lv_win_set_style(color_test.window, LV_WIN_STYLE_BG, &win_bg_test);

		static lv_style_t win_hdr_test;
		lv_style_copy(&win_hdr_test, lv_win_get_style(color_test.window, LV_WIN_STYLE_HEADER));
		win_hdr_test.body.main_color = bgc;
		win_hdr_test.body.grad_color = win_hdr_test.body.main_color;
		lv_win_set_style(color_test.window, LV_WIN_STYLE_HEADER, &win_hdr_test);

		static lv_style_t hdr1_bg_test;
		lv_style_copy(&hdr1_bg_test, lv_cont_get_style(color_test.header1));
		hdr1_bg_test.body.main_color = bgc;
		hdr1_bg_test.body.grad_color = hdr1_bg_test.body.main_color;
		lv_cont_set_style(color_test.header1, &hdr1_bg_test);

		static lv_style_t hdr2_bg_test;
		lv_style_copy(&hdr2_bg_test, lv_cont_get_style(color_test.header2));
		hdr2_bg_test.body.main_color = bgc;
		hdr2_bg_test.body.grad_color = hdr2_bg_test.body.main_color;
		lv_cont_set_style(color_test.header2, &hdr2_bg_test);

		static lv_style_t btn_tgl_rel_test;
		lv_style_copy(&btn_tgl_rel_test, lv_btn_get_style(color_test.btn_reset, LV_BTN_STATE_REL));
		btn_tgl_rel_test.body.main_color = bgc_light;
		btn_tgl_rel_test.body.grad_color = btn_tgl_rel_test.body.main_color;
		lv_btn_set_style(color_test.btn_reset, LV_BTN_STATE_REL, &btn_tgl_rel_test);
		lv_btn_set_style(color_test.btn_reset, LV_BTN_STATE_PR, &btn_tgl_pr_test);
		lv_btn_set_style(color_test.btn_apply, LV_BTN_STATE_REL, &btn_tgl_rel_test);
		lv_btn_set_style(color_test.btn_apply, LV_BTN_STATE_PR, &btn_tgl_pr_test);
		lv_btn_set_style(color_test.btn_black, LV_BTN_STATE_REL, &btn_tgl_rel_test);
		lv_btn_set_style(color_test.btn_black, LV_BTN_STATE_PR, &btn_tgl_pr_test);

		static lv_style_t slider_bg;
		lv_style_copy(&slider_bg, lv_slider_get_style(color_test.slider, LV_SLIDER_STYLE_BG));
		slider_bg.body.main_color = bg_border;
		slider_bg.body.grad_color = slider_bg.body.main_color;
		lv_slider_set_style(color_test.hue_slider, LV_SLIDER_STYLE_BG, &slider_bg);
		lv_slider_set_style(color_test.slider,     LV_SLIDER_STYLE_BG, &slider_bg);
		lv_slider_set_style(color_test.r_slider,   LV_SLIDER_STYLE_BG, &slider_bg);
		lv_slider_set_style(color_test.g_slider,   LV_SLIDER_STYLE_BG, &slider_bg);
		lv_slider_set_style(color_test.b_slider,   LV_SLIDER_STYLE_BG, &slider_bg);
	}
	else
	{
		static lv_style_t txt_test;
		lv_style_copy(&txt_test, lv_label_get_style(color_test.label));
		txt_test.text.color = color;
		lv_obj_set_style(color_test.label, &txt_test);
		lv_obj_set_style(color_test.icons, &txt_test);

		static lv_style_t slider_knb;
		lv_style_copy(&slider_knb, lv_slider_get_style(color_test.slider, LV_SLIDER_STYLE_KNOB));
		slider_knb.body.main_color = color;
		slider_knb.body.grad_color = slider_knb.body.main_color;
		lv_slider_set_style(color_test.hue_slider, LV_SLIDER_STYLE_KNOB, &slider_knb);
		lv_slider_set_style(color_test.slider,     LV_SLIDER_STYLE_KNOB, &slider_knb);
		lv_slider_set_style(color_test.r_slider,   LV_SLIDER_STYLE_KNOB, &slider_knb);
		lv_slider_set_style(color_test.g_slider,   LV_SLIDER_STYLE_KNOB, &slider_knb);
		lv_slider_set_style(color_test.b_slider,   LV_SLIDER_STYLE_KNOB, &slider_knb);

		static lv_style_t slider_ind;
		lv_style_copy(&slider_ind, lv_slider_get_style(color_test.slider, LV_SLIDER_STYLE_INDIC));
		slider_ind.body.main_color = hue ? lv_color_hsv_to_rgb(hue, 100, 72) : lv_color_hsv_to_rgb(53, 8, 65);
		slider_ind.body.grad_color = slider_ind.body.main_color;
		lv_slider_set_style(color_test.hue_slider, LV_SLIDER_STYLE_INDIC, &slider_ind);
		lv_slider_set_style(color_test.slider,     LV_SLIDER_STYLE_INDIC, &slider_ind);
		lv_slider_set_style(color_test.r_slider,   LV_SLIDER_STYLE_INDIC, &slider_ind);
		lv_slider_set_style(color_test.g_slider,   LV_SLIDER_STYLE_INDIC, &slider_ind);
		lv_slider_set_style(color_test.b_slider,   LV_SLIDER_STYLE_INDIC, &slider_ind);
	}

	lv_btn_set_style(color_test.button, LV_BTN_STATE_TGL_PR, &btn_tgl_pr_test);
}

static lv_res_t _slider_hue_action(lv_obj_t *slider)
{
	if (color_test.hue != lv_slider_get_value(slider))
	{
		color_test.hue = lv_slider_get_value(slider);

		_show_new_nyx_color(false);

		char hue[8];
		s_printf(hue, "%03d", color_test.hue);
		lv_label_set_text(color_test.hue_label, hue);
	}

	return LV_RES_OK;
}

static lv_res_t _slider_r_action(lv_obj_t *slider)
{
	if (color_test.r != lv_slider_get_value(slider))
	{
		color_test.r = lv_slider_get_value(slider);
		color_test.box_style.body.main_color = LV_COLOR_HEX((color_test.r << 16) | (color_test.g << 8) | color_test.b);
		color_test.box_style.body.grad_color = color_test.box_style.body.main_color;
		lv_obj_set_style(color_test.box, &color_test.box_style);

		char shade[8];
		s_printf(shade, "%03d", color_test.r);
		lv_label_set_text(color_test.r_label, shade);
	}

	return LV_RES_OK;
}

static lv_res_t _slider_g_action(lv_obj_t *slider)
{
	if (color_test.g != lv_slider_get_value(slider))
	{
		color_test.g = lv_slider_get_value(slider);
		color_test.box_style.body.main_color = LV_COLOR_HEX((color_test.r << 16) | (color_test.g << 8) | color_test.b);
		color_test.box_style.body.grad_color = color_test.box_style.body.main_color;
		lv_obj_set_style(color_test.box, &color_test.box_style);

		char shade[8];
		s_printf(shade, "%03d", color_test.g);
		lv_label_set_text(color_test.g_label, shade);
	}

	return LV_RES_OK;
}

static lv_res_t _slider_b_action(lv_obj_t *slider)
{
	if (color_test.b != lv_slider_get_value(slider))
	{
		color_test.b = lv_slider_get_value(slider);
		color_test.box_style.body.main_color = LV_COLOR_HEX((color_test.r << 16) | (color_test.g << 8) | color_test.b);
		color_test.box_style.body.grad_color = color_test.box_style.body.main_color;
		lv_obj_set_style(color_test.box, &color_test.box_style);

		char shade[8];
		s_printf(shade, "%03d", color_test.b);
		lv_label_set_text(color_test.b_label, shade);
	}

	return LV_RES_OK;
}

static lv_res_t _preset_bg_apply(lv_obj_t *btn)
{
	color_test.bg = (color_test.r << 16) | (color_test.g << 8) | color_test.b;

	_show_new_nyx_color(true);

	return LV_RES_OK;
}

static lv_res_t _preset_bg_black(lv_obj_t *btn)
{
	color_test.bg = 0;

	_show_new_nyx_color(true);

	return LV_RES_OK;
}

static lv_res_t _preset_bg_reset(lv_obj_t *btn)
{
	color_test.r = 0x2D;
	color_test.g = 0x2D;
	color_test.b = 0x2D;
	color_test.bg = 0x2D2D2D;

	color_test.box_style.body.main_color = LV_COLOR_HEX(color_test.bg);
	color_test.box_style.body.grad_color = color_test.box_style.body.main_color;
	lv_obj_set_style(color_test.box, &color_test.box_style);

	lv_bar_set_value(color_test.r_slider, color_test.r);
	lv_bar_set_value(color_test.g_slider, color_test.g);
	lv_bar_set_value(color_test.b_slider, color_test.b);

	char shade[8];
	s_printf(shade, "%03d", color_test.r);
	lv_label_set_text(color_test.r_label, shade);
	lv_label_set_text(color_test.g_label, shade);
	lv_label_set_text(color_test.b_label, shade);

	_show_new_nyx_color(true);

	return LV_RES_OK;
}

static lv_res_t _preset_hue_action(lv_obj_t *btn)
{
	lv_btn_ext_t *ext = lv_obj_get_ext_attr(btn);

	if (color_test.hue != ext->idx)
	{
		color_test.hue = ext->idx;

		char hue[8];
		s_printf(hue, "%03d", color_test.hue);
		lv_label_set_text(color_test.hue_label, hue);
		lv_bar_set_value(color_test.hue_slider, color_test.hue);

		_show_new_nyx_color(false);
	}

	return LV_RES_OK;
}

static const u16 theme_colors[18] = {
	0, 4, 13, 23, 33, 43, 54, 66, 89, 124, 167, 187, 200, 208, 231, 261, 291, 341
};

lv_res_t _create_window_nyx_colors(lv_obj_t *btn)
{
	lv_obj_t *win = nyx_create_standard_window(SYMBOL_HINT"  테마 색상 & 화면 밝기", NULL);
	if (close_btn)
	{
		lv_obj_del(close_btn);
		close_btn = NULL;
	}
	lv_win_add_btn(win, NULL, SYMBOL_SAVE" 저장 및 적용", _action_win_nyx_colors_save);
	color_test.window = win;

	// Set current theme colors.
	color_test.bg  = n_cfg.theme_bg;
	color_test.hue = n_cfg.theme_color;
	u32 bg = n_cfg.theme_bg ? n_cfg.theme_bg : 0x2D2D2D;
	color_test.r = (bg >> 16) & 0xFF;
	color_test.g = (bg >>  8) & 0xFF;
	color_test.b = (bg >>  0) & 0xFF;

	lv_style_copy(&color_test.box_style, &lv_style_plain_color);
	color_test.box_style.body.main_color = LV_COLOR_HEX(color_test.bg);
	color_test.box_style.body.grad_color = color_test.box_style.body.main_color;
	color_test.box_style.body.border.color = LV_COLOR_HEX(0xFFFFFF);
	color_test.box_style.body.border.opa   = LV_OPA_20;
	color_test.box_style.body.border.width = 2;

	// Create container to keep content inside.
	lv_obj_t *h1 = lv_cont_create(win, NULL);
	lv_obj_set_size(h1, LV_DPI * 299 / 25, LV_DPI * 27 / 26);
	color_test.header1 = h1;

	lv_obj_t *acc_label = lv_label_create(h1, NULL);
	lv_label_set_static_text(acc_label, "강조 색상:");

	// Create color preset buttons.
	lv_obj_t *color_btn = lv_btn_create(h1, NULL);
	lv_btn_ext_t *ext = lv_obj_get_ext_attr(color_btn);
	ext->idx = theme_colors[0];
	create_flat_button(color_btn, ext->idx, _preset_hue_action);
	lv_obj_align(color_btn, acc_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 10);
	lv_obj_t *color_btn2;

	for (u32 i = 1; i < ARRAY_SIZE(theme_colors); i++)
	{
		color_btn2 = lv_btn_create(h1, NULL);
		ext = lv_obj_get_ext_attr(color_btn2);
		ext->idx = theme_colors[i];
		create_flat_button(color_btn2, ext->idx, _preset_hue_action);
		lv_obj_align(color_btn2, color_btn, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
		color_btn = color_btn2;
	}

	lv_obj_align(h1, NULL, LV_ALIGN_IN_TOP_MID, 0, LV_DPI / 5);

	// Create hue slider.
	lv_obj_t *h_slider = lv_slider_create(win, NULL);
	lv_obj_set_width(h_slider, LV_DPI * 213 / 20);
	lv_obj_set_height(h_slider, LV_DPI * 4 / 10);
	lv_bar_set_range(h_slider, 0, 359);
	lv_bar_set_value(h_slider, color_test.hue);
	lv_slider_set_action(h_slider, _slider_hue_action);
	lv_obj_align(h_slider, h1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 5);
	color_test.hue_slider = h_slider;

	// Create hue label.
	lv_obj_t *hue_text_label = lv_label_create(win, NULL);
	lv_obj_align(hue_text_label, h_slider, LV_ALIGN_OUT_RIGHT_MID, LV_DPI * 24 / 100, 0);
	char txt[8];
	s_printf(txt, "%03d", color_test.hue);
	lv_label_set_text(hue_text_label, txt);
	color_test.hue_label = hue_text_label;

	lv_obj_t *bg_label = lv_label_create(win, NULL);
	lv_label_set_static_text(bg_label, "테마 색상:");
	lv_obj_align(bg_label, h_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI * 6 / 25);

	// Create red slider.
	lv_obj_t *r_slider = lv_slider_create(win, NULL);
	lv_obj_set_width(r_slider, LV_DPI * 85 / 16);
	lv_obj_set_height(r_slider, LV_DPI * 4 / 10);
	lv_bar_set_range(r_slider, 11, 100);
	lv_bar_set_value(r_slider, color_test.r);
	lv_slider_set_action(r_slider, _slider_r_action);
	lv_obj_align(r_slider, bg_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 21);
	color_test.r_slider = r_slider;

	// Create red label.
	lv_obj_t *r_text_label = lv_label_create(win, NULL);
	lv_obj_align(r_text_label, r_slider, LV_ALIGN_OUT_RIGHT_MID, LV_DPI * 24 / 100, 0);
	s_printf(txt, "%03d", color_test.r);
	lv_label_set_text(r_text_label, txt);
	color_test.r_label = r_text_label;

	// Create green slider.
	lv_obj_t *g_slider = lv_slider_create(win, r_slider);
	lv_bar_set_value(g_slider, color_test.g);
	lv_slider_set_action(g_slider, _slider_g_action);
	lv_obj_align(g_slider, r_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 7);
	color_test.g_slider = g_slider;

	// Create green label.
	lv_obj_t *g_text_label = lv_label_create(win, NULL);
	lv_obj_align(g_text_label, g_slider, LV_ALIGN_OUT_RIGHT_MID, LV_DPI * 24 / 100, 0);
	s_printf(txt, "%03d", color_test.g);
	lv_label_set_text(g_text_label, txt);
	color_test.g_label = g_text_label;

	// Create blue slider.
	lv_obj_t *b_slider = lv_slider_create(win, r_slider);
	lv_bar_set_value(b_slider, color_test.b);
	lv_slider_set_action(b_slider, _slider_b_action);
	lv_obj_align(b_slider, g_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 7);
	color_test.b_slider = b_slider;

	// Create blue label.
	lv_obj_t *b_text_label = lv_label_create(win, NULL);
	lv_obj_align(b_text_label, b_slider, LV_ALIGN_OUT_RIGHT_MID, LV_DPI * 24 / 100, 0);
	s_printf(txt, "%03d", color_test.b);
	lv_label_set_text(b_text_label, txt);
	color_test.b_label = b_text_label;

	// Create theme color box.
	lv_obj_t * bg_box = lv_obj_create(win, NULL);
	lv_obj_set_size(bg_box, LV_DPI * 10 / 7, LV_DPI * 18 / 13);
	lv_obj_align(bg_box, r_text_label, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 4, 0);
	lv_obj_set_style(bg_box, &color_test.box_style);
	color_test.box = bg_box;

	// Create theme color buttons.
	lv_obj_t *btn_reset = lv_btn_create(win, NULL);
	lv_obj_t *label_btn = lv_label_create(btn_reset, NULL);
	lv_label_set_static_text(label_btn, SYMBOL_REFRESH" Grey");
	lv_btn_set_fit(btn_reset, false, true);
	lv_obj_set_width(btn_reset, LV_DPI * 5 / 3);
	lv_btn_set_action(btn_reset, LV_BTN_ACTION_CLICK, _preset_bg_reset);
	lv_obj_align(btn_reset, bg_box, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 5, 0);
	color_test.btn_reset = btn_reset;

	lv_obj_t *btn_black = lv_btn_create(win, btn_reset);
	label_btn = lv_label_create(btn_black, NULL);
	lv_label_set_static_text(label_btn, SYMBOL_BRIGHTNESS" Black");
	lv_btn_set_action(btn_black, LV_BTN_ACTION_CLICK, _preset_bg_black);
	lv_obj_align(btn_black, btn_reset, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 5, 0);
	color_test.btn_black = btn_black;

	lv_obj_t *btn_apply = lv_btn_create(win, btn_reset);
	label_btn = lv_label_create(btn_apply, NULL);
	lv_label_set_static_text(label_btn, SYMBOL_LIST" 배경 색상");
	lv_obj_set_width(btn_apply, LV_DPI * 10 / 3 + LV_DPI / 5);
	lv_btn_set_action(btn_apply, LV_BTN_ACTION_CLICK, _preset_bg_apply);
	lv_obj_align(btn_apply, btn_reset, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 7);
	color_test.btn_apply = btn_apply;

	// Create sample text.
	lv_obj_t *h2 = lv_cont_create(win, NULL);
	lv_obj_set_size(h2, LV_DPI * 12, LV_DPI * 18 / 10);
	lv_obj_align(h2, b_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI * 6 / 25);
	color_test.header2 = h2;

	lv_obj_t *lbl_sample = lv_label_create(h2, NULL);
	lv_label_set_static_text(lbl_sample, "색상 샘플:");

	lv_obj_t *lbl_test = lv_label_create(h2, NULL);
	lv_label_set_long_mode(lbl_test, LV_LABEL_LONG_BREAK);
	lv_label_set_static_text(lbl_test,
		"현재 Ｌ은 Asa의 프로젝트에서 포크되었습니다.\n"
		"Mariko 전용으로 설계되었으며, 개인 사용만을 목적으로합니다.\n"
		"이를 어기고 무단 사용으로 인해 발생한 문제에 대하여 일절 책임지지 않습니다.\n\n"
		"3초 이상 입력을 유지하여 숨겨진 메뉴를 사용 가능합니다.\n"
		"포맷·분할:  eMMC 분할 모드   |   Ｘ:  RAM 전환   |   Ｅ:  페이로드 부팅   |   Ｕ:  HID 모드");
	lv_obj_set_width(lbl_test, lv_obj_get_width(h2) - LV_DPI * 6 / 10);
	lv_obj_align(lbl_test, lbl_sample, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 5);
	color_test.label = lbl_test;

	// Create sample icons.
	lv_obj_t *lbl_icons = lv_label_create(h2, NULL);
	lv_label_set_static_text(lbl_icons,
		SYMBOL_BRIGHTNESS SYMBOL_CHARGE SYMBOL_FILE SYMBOL_DRIVE SYMBOL_FILE_CODE
		SYMBOL_EDIT SYMBOL_HINT SYMBOL_DRIVE SYMBOL_KEYBOARD SYMBOL_POWER);
	lv_obj_align(lbl_icons, lbl_test, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI * 2 / 5);
	color_test.icons = lbl_icons;

	// Create Backlight slider.
	lv_obj_t *label_txt = lv_label_create(h2, NULL);
	lv_label_set_static_text(label_txt, SYMBOL_BRIGHTNESS" 화면 밝기:");
	lv_obj_align(label_txt, lbl_icons, LV_ALIGN_OUT_RIGHT_MID, 100, 0);

	// Create sample slider.
	lv_obj_t *slider_bl = lv_slider_create(h2, NULL);
	lv_bar_set_range(slider_bl, 30, 220);
	lv_bar_set_value(slider_bl, h_cfg.backlight);
	lv_slider_set_action(slider_bl, _slider_brightness_action);
	lv_obj_align(slider_bl, label_txt, LV_ALIGN_OUT_RIGHT_MID, 30, 0);
	color_test.slider = slider_bl;

	// Create sample button.
	lv_obj_t *btn_test = lv_btn_create(h2, NULL);
	lv_btn_set_state(btn_test, LV_BTN_STATE_TGL_PR);
	lv_obj_align(btn_test, lbl_test, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, LV_DPI / 5);
	lv_label_create(btn_test, NULL);
	lv_obj_set_click(btn_test, false);
	color_test.button = btn_test;

	_show_new_nyx_color(color_test.hue);

	return LV_RES_OK;
}

typedef struct _time_edit_obj_t
{
	lv_obj_t *year;
	lv_obj_t *month;
	lv_obj_t *day;
	lv_obj_t *hour;
	lv_obj_t *min;
} time_edit_obj_t;

time_edit_obj_t clock_ctxt;

static lv_res_t _action_clock_edit(lv_obj_t *btns, const char * txt)
{
	int btn_idx = lv_btnm_get_pressed(btns);

	if (btn_idx == 1)
	{
		rtc_time_t time;
		max77620_rtc_get_time(&time);
		u32 epoch = max77620_rtc_date_to_epoch(&time);

		u32 year  = lv_roller_get_selected(clock_ctxt.year) + CLOCK_MIN_YEAR;
		u32 month = lv_roller_get_selected(clock_ctxt.month) + 1;
		u32 day   = lv_roller_get_selected(clock_ctxt.day) + 1;
		u32 hour  = lv_roller_get_selected(clock_ctxt.hour);
		u32 min   = lv_roller_get_selected(clock_ctxt.min);

		switch (month)
		{
		case 2:
			if (!(year % 4) && day > 29)
				day = 29;
			else if (day > 28)
				day = 28;
			break;
		case 4:
		case 6:
		case 9:
		case 11:
			if (day > 30)
				day = 30;
			break;
		}

		time.year  = year;
		time.month = month;
		time.day   = day;
		time.hour  = hour;
		time.min   = min;

		u32 new_epoch = max77620_rtc_date_to_epoch(&time);

		// Stored in u32 and allow overflow for integer offset casting.
		n_cfg.timeoffset = new_epoch - epoch;

		// If canceled set 1 for invalidating first boot clock edit.
		if (!n_cfg.timeoffset)
			n_cfg.timeoffset = 1;
		else
		{
			// Adjust for DST between 28 march and 28 october.
			// Good enough to cover all years as week info is not valid.
			u16 md = (time.month << 8) | time.day;
			if (n_cfg.timedst && md >= 0x31C && md < 0xA1C)
				n_cfg.timeoffset -= 3600; // Store time in non DST.
			max77620_rtc_set_epoch_offset((int)n_cfg.timeoffset);
		}

		nyx_changes_made = true;
	} else if (btn_idx == 2 && timeoffset_backup_valid) {
		n_cfg.timeoffset = timeoffset_backup;
		max77620_rtc_set_epoch_offset((int)n_cfg.timeoffset);
	}

	timeoffset_backup_valid = false;

	nyx_mbox_action(btns, txt);

	return LV_RES_INV;
}

static lv_res_t _action_clock_edit_save(lv_obj_t *btns, const char * txt)
{
	_action_clock_edit(btns, txt);

	// Save if changes were made.
	if (nyx_changes_made)
		_save_nyx_options_action(NULL);

	return LV_RES_INV;
}

static lv_res_t _action_auto_dst_toggle(lv_obj_t *btn)
{
	n_cfg.timedst = !n_cfg.timedst;
	max77620_rtc_set_auto_dst(n_cfg.timedst);

	if (!n_cfg.timedst)
		lv_btn_set_state(btn, LV_BTN_STATE_REL);
	else
		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);

	nyx_generic_onoff_toggle(btn);

	return LV_RES_OK;
}

static const u32 month_days[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static lv_res_t _action_date_validation(lv_obj_t *roller)
{
	u32 year  = lv_roller_get_selected(clock_ctxt.year) + CLOCK_MIN_YEAR;
	u32 month = lv_roller_get_selected(clock_ctxt.month) + 1;
	u32 day   = lv_roller_get_selected(clock_ctxt.day) + 1;

	// Adjust max day based on year and month.
	u32 max_mon_day = month_days[month - 1];
	u32 max_feb_day = !(year % 4) ? 29 : 28;
	if (month == 2 && day > max_feb_day)
		lv_roller_set_selected(clock_ctxt.day, max_feb_day - 1, false);
	else if (day > max_mon_day)
		lv_roller_set_selected(clock_ctxt.day, max_mon_day - 1, false);

	return LV_RES_OK;
}

lv_res_t _create_mbox_clock_edit(lv_obj_t *btn)
{
	timeoffset_backup = n_cfg.timeoffset;
	timeoffset_backup_valid = true;

	static lv_style_t mbox_style;
	lv_theme_t *th = lv_theme_get_current();
	lv_style_copy(&mbox_style, th->mbox.bg);
	mbox_style.body.padding.inner = LV_DPI / 10;

	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "\222적용", "\222취소", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_style(mbox, LV_MBOX_STYLE_BG, &mbox_style);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 6);

	lv_mbox_set_text(mbox,
		"#008EED Ｃ 날짜 및 시간#\n\n"
		"#FFBA00 안내#: Ｈ 시스템 시간을 설정합니다.\n"
		"본체의 시간과는 동기화되지 않습니다.");

	lv_obj_t *padding = lv_cont_create(mbox, NULL);
	lv_cont_set_fit(padding, true, false);
	lv_cont_set_style(padding, &lv_style_transp);
	lv_obj_set_height(padding, LV_DPI / 10);

	// Get current time.
	rtc_time_t time;
	max77620_rtc_get_time_adjusted(&time);

	// Normalize year if out of range.
	if (time.year < CLOCK_MIN_YEAR)
		time.year = CLOCK_MIN_YEAR;
	else if (time.year > CLOCK_MAX_YEAR)
		time.year = CLOCK_MAX_YEAR;

	time.year -= CLOCK_MIN_YEAR;

	lv_obj_t *h1 = lv_cont_create(mbox, NULL);
	lv_cont_set_fit(h1, true, true);

	// Create year roller.
	lv_obj_t *roller_year = lv_roller_create(h1, NULL);
	lv_roller_set_options(roller_year, CLOCK_YEARLIST);
	lv_roller_set_selected(roller_year, time.year, false);
	lv_roller_set_visible_row_count(roller_year, 3);
	lv_roller_set_action(roller_year, _action_date_validation);
	clock_ctxt.year = roller_year;

	// Create month roller.
	lv_obj_t *roller_month = lv_roller_create(h1, roller_year);
	lv_roller_set_options(roller_month,
		"1월\n"
		"2월\n"
		"3월\n"
		"4월\n"
		"5월\n"
		"6월\n"
		"7월\n"
		"8월\n"
		"9월\n"
		"10월\n"
		"11월\n"
		"12월");
	lv_roller_set_selected(roller_month, time.month - 1, false);
	lv_obj_align(roller_month, roller_year, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
	lv_roller_set_action(roller_month, _action_date_validation);
	clock_ctxt.month = roller_month;

	// Create day roller.
	static char days[256];
	days[0] = 0;
	for (u32 i = 1; i < 32; i++)
		s_printf(days + strlen(days), " %d일 \n", i);
	days[strlen(days) - 1] = 0;
	lv_obj_t *roller_day = lv_roller_create(h1, roller_year);
	lv_roller_set_options(roller_day, days);
	lv_roller_set_selected(roller_day, time.day - 1, false);
	lv_roller_set_action(roller_day, _action_date_validation);
	lv_obj_align(roller_day, roller_month, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
	clock_ctxt.day = roller_day;

	// Create hours roller.
	static char hours[256];
	hours[0] = 0;
	for (u32 i = 0; i < 24; i++)
		s_printf(hours + strlen(hours), " %d시 \n", i);
	hours[strlen(hours) - 1] = 0;
	lv_obj_t *roller_hour = lv_roller_create(h1, roller_year);
	lv_roller_set_options(roller_hour, hours);
	lv_roller_set_selected(roller_hour, time.hour, false);
	lv_obj_align(roller_hour, roller_day, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 2, 0);
	clock_ctxt.hour = roller_hour;

	// Create minutes roller.
	static char minutes[512];
	minutes[0] = 0;
	for (u32 i = 0; i < 60; i++)
		s_printf(minutes + strlen(minutes), " %02d분 \n", i);
	minutes[strlen(minutes) - 1] = 0;
	lv_obj_t *roller_minute = lv_roller_create(h1, roller_year);
	lv_roller_set_options(roller_minute, minutes);
	lv_roller_set_selected(roller_minute, time.min, false);
	lv_obj_align(roller_minute, roller_hour, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
	clock_ctxt.min = roller_minute;

	// Add DST option.
	lv_obj_t *btn_dst = lv_btn_create(mbox, NULL);
	nyx_create_onoff_button(th, h1, btn_dst, SYMBOL_BRIGHTNESS" 서머 타임 자동 적용", _action_auto_dst_toggle, true);
	if (n_cfg.timedst)
		lv_btn_set_state(btn_dst, LV_BTN_STATE_TGL_REL);
	nyx_generic_onoff_toggle(btn_dst);

	// If btn is empty, save options also because it was launched from boot.
	lv_mbox_add_btns(mbox, mbox_btn_map, _action_clock_edit_save);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

void first_time_clock_edit(void *param)
{
	_create_mbox_clock_edit(NULL);
}

lv_res_t _joycon_info_dump_action(lv_obj_t * btn)
{
	FIL fp;
	int error = 0;
	int cal_error = 0;
	bool is_l_hos = false;
	bool is_r_hos = false;
	u32 joycon_found = 0;
	bool nx_hoag = fuse_read_hw_type() == FUSE_NX_HW_TYPE_HOAG;
	jc_gamepad_rpt_t *jc_pad = jc_get_bt_pairing_info(&is_l_hos, &is_r_hos);

	char *data = (char *)malloc(SZ_16K);
	char *txt_buf = (char *)malloc(SZ_4K);

	if (!nx_hoag && !jc_pad)
		error = 255;

	// Try 2 times to get factory calibration data.
	for (u32 i = 0; i < 2; i++)
	{
		if (!error)
			cal_error = hos_dump_cal0();
		if (!cal_error)
			break;
	}

	if (cal_error && nx_hoag)
		error = cal_error;

	if (error)
		goto disabled_or_cal0_issue;

	if (nx_hoag)
		goto save_data;

	// Count valid joycon.
	joycon_found = jc_pad->bt_conn_l.type ? 1 : 0;
	if (jc_pad->bt_conn_r.type)
		joycon_found++;

	// Reset PC based for dumping.
	jc_pad->bt_conn_l.type = is_l_hos ? jc_pad->bt_conn_l.type : 0;
	jc_pad->bt_conn_r.type = is_r_hos ? jc_pad->bt_conn_r.type : 0;

save_data:
	error = sd_mount() ? 5 : 0;

	if (!error)
	{
		nx_emmc_cal0_t *cal0 = (nx_emmc_cal0_t *)cal0_buf;

		f_mkdir("switchroot");

		if (!nx_hoag)
		{
			// Save binary dump.
			memcpy(data, &jc_pad->bt_conn_l, sizeof(jc_bt_conn_t));
			memcpy(data + sizeof(jc_bt_conn_t), &jc_pad->bt_conn_r, sizeof(jc_bt_conn_t));

			error = sd_save_to_file((u8 *)data, sizeof(jc_bt_conn_t) * 2, "switchroot/joycon_mac.bin") ? 4 : 0;

			// Save readable dump.
			data[0] = 0;
			for (u32 i = 0; i < 2; i++)
			{
				jc_bt_conn_t *bt = !i ? &jc_pad->bt_conn_l : &jc_pad->bt_conn_r;
				s_printf(data + strlen(data),
					"[joycon_0%d]\ntype=%d\nmac=%02X:%02X:%02X:%02X:%02X:%02X\n"
					"host=%02X:%02X:%02X:%02X:%02X:%02X\n"
					"ltk=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n\n",
					i, bt->type, bt->mac[0], bt->mac[1], bt->mac[2], bt->mac[3], bt->mac[4], bt->mac[5],
					bt->host_mac[0], bt->host_mac[1], bt->host_mac[2], bt->host_mac[3], bt->host_mac[4], bt->host_mac[5],
					bt->ltk[0], bt->ltk[1], bt->ltk[2], bt->ltk[3], bt->ltk[4], bt->ltk[5], bt->ltk[6], bt->ltk[7],
					bt->ltk[8], bt->ltk[9], bt->ltk[10], bt->ltk[11], bt->ltk[12], bt->ltk[13], bt->ltk[14], bt->ltk[15]);
			}

			if (!error)
				error = f_open(&fp, "switchroot/joycon_mac.ini", FA_WRITE | FA_CREATE_ALWAYS) ? 4 : 0;
			if (!error)
			{
				f_puts(data, &fp);
				f_close(&fp);
			}

			// Save IMU Calibration data.
			if (!error && !cal_error)
			{
				s_printf(data,
					"imu_type=%d\n\n"
					"acc_cal_off_x=0x%X\n"
					"acc_cal_off_y=0x%X\n"
					"acc_cal_off_z=0x%X\n"
					"acc_cal_scl_x=0x%X\n"
					"acc_cal_scl_y=0x%X\n"
					"acc_cal_scl_z=0x%X\n\n"

					"gyr_cal_off_x=0x%X\n"
					"gyr_cal_off_y=0x%X\n"
					"gyr_cal_off_z=0x%X\n"
					"gyr_cal_scl_x=0x%X\n"
					"gyr_cal_scl_y=0x%X\n"
					"gyr_cal_scl_z=0x%X\n\n"

					"device_bt_mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
					cal0->console_6axis_sensor_type,
					cal0->acc_offset[0],  cal0->acc_offset[1],  cal0->acc_offset[2],
					cal0->acc_scale[0],   cal0->acc_scale[1],   cal0->acc_scale[2],
					cal0->gyro_offset[0], cal0->gyro_offset[1], cal0->gyro_offset[2],
					cal0->gyro_scale[0],  cal0->gyro_scale[1],  cal0->gyro_scale[2],
					cal0->bd_mac[0], cal0->bd_mac[1], cal0->bd_mac[2], cal0->bd_mac[3], cal0->bd_mac[4], cal0->bd_mac[5]);

				error = f_open(&fp, "switchroot/switch.cal", FA_WRITE | FA_CREATE_ALWAYS) ? 4 : 0;
				if (!error)
				{
					f_puts(data, &fp);
					f_close(&fp);
				}
			}
		}
		else
		{
			jc_calib_t *stick_cal_l = (jc_calib_t *)cal0->analog_stick_cal_l;
			jc_calib_t *stick_cal_r = (jc_calib_t *)cal0->analog_stick_cal_r;

			// Save Lite Gamepad and IMU Calibration data.
			// Actual max/min are right/left and up/down offsets.
			// Sticks: 0x23: H1 (Hosiden), 0x25: H5 (Hosiden), 0x41: F1 (FIT), ?add missing?
			s_printf(data,
				"lite_cal_l_type=0x%X\n"
				"lite_cal_lx_lof=0x%X\n"
				"lite_cal_lx_cnt=0x%X\n"
				"lite_cal_lx_rof=0x%X\n"
				"lite_cal_ly_dof=0x%X\n"
				"lite_cal_ly_cnt=0x%X\n"
				"lite_cal_ly_uof=0x%X\n\n"

				"lite_cal_r_type=0x%X\n"
				"lite_cal_rx_lof=0x%X\n"
				"lite_cal_rx_cnt=0x%X\n"
				"lite_cal_rx_rof=0x%X\n"
				"lite_cal_ry_dof=0x%X\n"
				"lite_cal_ry_cnt=0x%X\n"
				"lite_cal_ry_uof=0x%X\n\n"

				"imu_type=%d\n\n"
				"acc_cal_off_x=0x%X\n"
				"acc_cal_off_y=0x%X\n"
				"acc_cal_off_z=0x%X\n"
				"acc_cal_scl_x=0x%X\n"
				"acc_cal_scl_y=0x%X\n"
				"acc_cal_scl_z=0x%X\n\n"

				"gyr_cal_off_x=0x%X\n"
				"gyr_cal_off_y=0x%X\n"
				"gyr_cal_off_z=0x%X\n"
				"gyr_cal_scl_x=0x%X\n"
				"gyr_cal_scl_y=0x%X\n"
				"gyr_cal_scl_z=0x%X\n\n"

				"device_bt_mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
				cal0->analog_stick_type_l,
				stick_cal_l->x_min, stick_cal_l->x_center, stick_cal_l->x_max,
				stick_cal_l->y_min, stick_cal_l->y_center, stick_cal_l->y_max,
				cal0->analog_stick_type_r,
				stick_cal_r->x_min, stick_cal_r->x_center, stick_cal_r->x_max,
				stick_cal_r->y_min, stick_cal_r->y_center, stick_cal_r->y_max,
				cal0->console_6axis_sensor_type,
				cal0->acc_offset[0],  cal0->acc_offset[1],  cal0->acc_offset[2],
				cal0->acc_scale[0],   cal0->acc_scale[1],   cal0->acc_scale[2],
				cal0->gyro_offset[0], cal0->gyro_offset[1], cal0->gyro_offset[2],
				cal0->gyro_scale[0],  cal0->gyro_scale[1],  cal0->gyro_scale[2],
				cal0->bd_mac[0], cal0->bd_mac[1], cal0->bd_mac[2], cal0->bd_mac[3], cal0->bd_mac[4], cal0->bd_mac[5]);
			if (!error)
				error = f_open(&fp, "switchroot/switch.cal", FA_WRITE | FA_CREATE_ALWAYS) ? 4 : 0;
			if (!error)
			{
				f_puts(data, &fp);
				f_close(&fp);
			}
		}

		sd_unmount();
	}

disabled_or_cal0_issue:;
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char *mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);

	if (!error)
	{
		if (!nx_hoag)
		{
			s_printf(txt_buf, "#008EED Ｊ 조이콘 페어링 데이터#\n\n");

			bool success = true;

			// Check if pairing info was found.
			if (joycon_found == 2)
				strcat(txt_buf,
					"#FFBA00 안내#: 다음 경로에 저장되었습니다.\n\n"
					"#C7EA46 sdmc:/switchroot/#\n"
					"#C7EA46 joycon_mac.bin, joycon_mac.ini, switch.cal#");
			else
			{
				s_printf(txt_buf + strlen(txt_buf),
						"#FFBA00 안내#: 다음 경로에 #008EED %d#개 데이터가 저장되었습니다.\n\n"
						"#C7EA46 sdmc:/switchroot/#\n"
						"#C7EA46 joycon_mac.bin, joycon_mac.ini, switch.cal#\n\n", joycon_found);
				success = false;
			}

			// Check if pairing was done in HOS.
			if (is_l_hos && is_r_hos)
				strcat(txt_buf, "");
			else if (!is_l_hos && is_r_hos)
			{
				strcat(txt_buf, "#FF8000 경고#: #FFBA00 좌측 조이콘#이 연결되어 있지 않습니다!");
				success = false;
			}
			else if (is_l_hos && !is_r_hos)
			{
				strcat(txt_buf, "#FF8000 경고#: #FFBA00 우측 조이콘#이 연결되어 있지 않습니다!");
				success = false;
			}
			else
			{
				strcat(txt_buf, "#FF8000 경고#: #FFBA00 조이콘이 연결되어 있지 않습니다!#");
				success = false;
			}

			if (!success)
				strcat(txt_buf,
					"\n#FFBA00 조이콘이 세트로 등록되어 있는지 확인하세요!#");

			if (cal_error)
				s_printf(txt_buf + strlen(txt_buf), "\n\n#FF8000 오류 (%d)#: IMU 보정 값을 읽을 수 없습니다!#", cal_error);
		}
		else
		{
			s_printf(txt_buf,
				"#008EED Lite 컨트롤러 페어링 데이터#\n\n"
				"#FFBA00 안내#: 다음 경로에 저장되었습니다.\n\n"
				"#C7EA46 sdmc:/switchroot/switch.cal#\n");
		}
	}
	else
	{
		if (!nx_hoag)
			s_printf(txt_buf,
				"#008EED 조이콘 페어링 데이터#\n\n"
				"#FF8000 오류 (%d)#: 페어링 데이터 저장에 실패했습니다!", error);
		else
			s_printf(txt_buf,
				"#008EED Lite 컨트롤러 페어링 데이터#\n\n"
				"#FF8000 오류 (%d)#: 페어링 데이터 저장에 실패했습니다!", error);
	}

	lv_mbox_set_text(mbox, txt_buf);

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action); // Important. After set_text.

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	free(txt_buf);
	free(data);

	return LV_RES_OK;
}

//=================================
//  ASAP: PIN lock configuration.
//=================================
lv_obj_t *set_pw_area;
static lv_res_t _set_passwd_ta_action(lv_obj_t *btnm, const char *txt)
{
	if (!txt) return LV_RES_OK;
	if (strcmp(txt, SYMBOL_REBOOT) == 0) {
		lv_ta_set_text(set_pw_area, "");
		return LV_RES_OK;
	}
	if (strcmp(txt, "Ｄ") == 0) {
		lv_ta_del_char(set_pw_area);
		return LV_RES_OK;
	}
	char c = txt[0];
	if (c < '0' || c > '9') return LV_RES_OK;
	lv_ta_set_cursor_pos(set_pw_area, LV_TA_CURSOR_LAST);
	lv_ta_add_text(set_pw_area, txt);
	return LV_RES_OK;
}

static lv_res_t _set_passwd_action(lv_obj_t *btns, const char *txt)
{
	u32 btnidx = lv_btnm_get_pressed(btns);

	switch (btnidx)
	{
	case 0: {
		const char *passwd = lv_ta_get_text(set_pw_area);
		if (passwd[0] == '\0') {
			n_cfg.pinlock[0] = '\0';
			nyx_changes_made = true;
			break;
		}
		strncpy(n_cfg.pinlock, passwd, sizeof(n_cfg.pinlock));
		n_cfg.pinlock[sizeof(n_cfg.pinlock)-1] = '\0';
		nyx_changes_made = true;
		break;
	}
	case 1:
		// disable, set pinlock to 0
		n_cfg.pinlock[0] = '\0';
		nyx_changes_made = true;
		break;
	}

	return nyx_mbox_action(btns, txt);
}

static lv_res_t _pinlock_edit_save(lv_obj_t *btns, const char * txt)
{
	_set_passwd_action(btns, txt);

	// Save if changes were made.
	if (nyx_changes_made)
		_save_nyx_options_action(NULL);

	return LV_RES_INV;
}

lv_res_t _action_win_nyx_options_passwd(lv_obj_t *btn)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\221적용", "\221비활성화", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 2);

	lv_mbox_set_text(mbox, "PIN 설정 [ #FF8000 최대 8자리# ]");

	set_pw_area = lv_ta_create(mbox, NULL);
	lv_ta_set_one_line(set_pw_area, true);
	lv_ta_set_pwd_mode(set_pw_area, false);
	// makes ta_add_... not work
	// lv_ta_set_accepted_chars(pw_area, "0123456789");
	lv_ta_set_accepted_chars(set_pw_area, NULL);
	lv_ta_set_cursor_type(set_pw_area, LV_CURSOR_BLOCK | LV_CURSOR_HIDDEN);
	lv_ta_set_max_length(set_pw_area, 8);
	lv_obj_set_width(set_pw_area, LV_HOR_RES / 5);
	if (n_cfg.pinlock[0])
	{
		lv_ta_set_text(set_pw_area, n_cfg.pinlock);
	}
	else
	{
		lv_ta_set_text(set_pw_area, "");
	}

	static const char * mbox_btnm_map[] = {
		"1", "2", "3", "\n",
		"4", "5", "6", "\n",
		"7", "8", "9", "\n",
		SYMBOL_REBOOT, "0", "Ｄ", "" };
	lv_obj_t *btnm1 = lv_btnm_create(mbox, NULL);
	lv_btnm_set_map(btnm1, mbox_btnm_map);
	lv_btnm_set_action(btnm1, _set_passwd_ta_action);
	lv_obj_set_size(btnm1, LV_HOR_RES / 3, LV_VER_RES / 4);

	lv_mbox_add_btns(mbox, mbox_btn_map, _pinlock_edit_save);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}
//=================================
