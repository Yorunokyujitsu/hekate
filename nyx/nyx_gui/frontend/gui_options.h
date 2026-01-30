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

#ifndef _GUI_OPTIONS_H_
#define _GUI_OPTIONS_H_

#include <libs/lvgl/lvgl.h>

void nyx_options_clear_ini_changes_made();
void first_time_clock_edit(void *param);
lv_res_t create_win_nyx_options(lv_obj_t *parrent_btn);

//==========================================================
//  ASAP: Joy-Con BT Dump. (gui_tools_partition_manager.c)
//==========================================================
lv_res_t _joycon_info_dump_action(lv_obj_t * btn);
//=================================================
//  ASAP: Data Verification. (gui_enummc_tools.c)
//=================================================
lv_res_t _data_verification_action(lv_obj_t *ddlist);
//====================================================================
//  ASAP: TimeCal, boot delay, PIN setting, nyx theme color. (gui.c)
//====================================================================
lv_res_t _create_mbox_clock_edit(lv_obj_t *btn);
lv_res_t _autoboot_delay_action(lv_obj_t *ddlist);
lv_res_t _action_win_nyx_options_passwd(lv_obj_t *btn);
lv_res_t _autoboot_list_action(lv_obj_t *ddlist);
lv_res_t _create_window_nyx_colors(lv_obj_t *btn);
//==========================================================

#endif
