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

#ifndef _GUI_H_
#define _GUI_H_

#include <libs/lvgl/lvgl.h>

typedef struct _emmc_tool_gui_t
{
	lv_obj_t *label_log;
	lv_obj_t *label_info;
	lv_obj_t *label_pct;
	lv_obj_t *label_finish;
	lv_obj_t *bar;
	lv_style_t *bar_teal_bg;
	lv_style_t *bar_teal_ind;
	lv_style_t *bar_white_bg;
	lv_style_t *bar_white_ind;
	lv_style_t *bar_orange_bg;
	lv_style_t *bar_orange_ind;
	char *txt_buf;
	char *base_path;
	bool raw_emummc;
} emmc_tool_gui_t;

typedef struct _gui_status_bar_ctx
{
	lv_obj_t *mid;
	lv_obj_t *battery;
	lv_obj_t *battery_more;
	//====================================
	//  ASAP: fix time, cal, temp label.
	//====================================
	lv_obj_t *time_btn;
	lv_obj_t *time_label;
	lv_obj_t *ampm_label;
	lv_obj_t *cal_label;
	lv_obj_t *temp_label;
	//====================================
} gui_status_bar_ctx;

//==================================
//  ASAP: PWR, VOL buttons config.
//==================================
typedef enum {
	GUI_PV_BTN_0 = 0,
	GUI_PV_BTN_1,
	GUI_PV_BTN_2,
	GUI_PV_BTN_3,
	GUI_PV_BTN_4
} gui_pv_btn_t;
//==================================

extern lv_style_t hint_small_style;
extern lv_style_t hint_small_style_white;
extern lv_style_t monospace_text;

extern lv_obj_t *payload_list;
extern lv_obj_t *autorcm_btn;
extern lv_obj_t *close_btn;

//=====================================
//  ASAP: Main Buttons global config.
//=====================================
extern lv_obj_t *atmo_bg_obj;
extern lv_obj_t *atmo_sphere_obj;
extern lv_obj_t *nandmng_label;
extern lv_obj_t *nandmng_color_labels[6];
extern lv_obj_t *nandmng_format_label;
extern lv_obj_t *nandmng_ftype_label;
extern lv_obj_t *label_status_obj;
extern lv_obj_t *label_nand_obj;
extern lv_obj_t *btn_toggle_emu_obj;
extern lv_obj_t *btn_emuenabled_obj;
//=====================================

extern const lv_img_dsc_t *icon_switch;
extern const lv_img_dsc_t *icon_payload;
extern lv_img_dsc_t *icon_lakka;

extern const lv_img_dsc_t *hekate_bg;

extern lv_style_t btn_transp_rel, btn_transp_pr, btn_transp_tgl_rel, btn_transp_tgl_pr, btn_transp_ina;
extern lv_style_t ddlist_transp_bg, ddlist_transp_sel;

//===================================
//  ASAP: New transp global config.
//===================================
extern lv_style_t btn_custom_rel, btn_custom_pr, btn_custom_pr2, btn_moon_pr;
//===================================

extern lv_style_t mbox_darken;

extern char *text_color;

extern gui_status_bar_ctx status_bar;

void reload_nyx(lv_obj_t *obj, bool force);
lv_img_dsc_t *bmp_to_lvimg_obj(const char *path);
bool nyx_emmc_check_battery_enough();
lv_res_t nyx_mbox_action(lv_obj_t * btns, const char * txt);
lv_res_t nyx_win_close_action(lv_obj_t * btn);
void nyx_window_toggle_buttons(lv_obj_t *win, bool disable);
lv_obj_t *nyx_create_standard_window(const char *win_title, lv_action_t close_action);
void nyx_create_onoff_button(lv_theme_t *th, lv_obj_t *parent, lv_obj_t *btn, const char *btn_name, lv_action_t action, bool transparent);
lv_res_t nyx_generic_onoff_toggle(lv_obj_t *btn);
void manual_system_maintenance(bool refresh);
void nyx_load_and_run();

//====================================================================================
//  DUALNAND Manager menu, Toggle the ‘enabled’ value of emuMMC. (fe_emummc_tools.c)
//====================================================================================
lv_obj_t *nyx_create_nand_manager_window(const char *win_title);
void refresh_nand_info_label(void);
void refresh_emu_enabled_label(void);
//===========================================================================
//  fe_emmc_tools.c, gui_info.c, gui_tools_partition_manager.c, gui_tools.c
//===========================================================================
const char *gui_pv_btn(gui_pv_btn_t type);
const char *gui_pv_btn_pair(gui_pv_btn_t a, gui_pv_btn_t b);
//====================================================================================

#endif
