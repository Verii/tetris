/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "conf.h"
#include "logs.h"
#include "helpers.h"

static const char config_defaults[] =
	/* Default movement keys */
	"bind move_drop 'w'\n"
	"bind move_left 'a'\n"
	"bind move_down 's'\n"
	"bind move_right 'd'\n"
	"bind rotate_left 'q'\n"
	"bind rotate_right 'e'\n"
	"bind hold_key 'h'\n"
	"bind pause_key 'p'\n"
	"bind quit_key 'o'\n"

#if 0
	/* Ingame toggle options */
	"bind toggle_ghosts 'g'\n"
	"bind toggle_wallkicks 'k'\n"

	"bind cycle_gamemodes 'm'\n"
#endif

	"set username \"username\"\n"

	"set logs_file \"~/.local/share/tetris/logs\"\n"
	"set save_file \"~/.local/share/tetris/saves\"\n"

	"set _conf_file \"~/.config/tetris/tetris.conf\"\n"
	;


int conf_create(struct config **retc)
{
	struct config *conf;

	conf = malloc(sizeof *conf);
	if (conf == NULL) {
		log_err("Out of memory\n");
		return -1;
	}

	*retc = conf;

	return 1;
}

/* Set default global variables, create necessary directories,
 * read in user specified configuration files.
 */
int conf_init(struct config *conf, const char *path)
{
	if (conf == NULL)
		return -1;

	if (path == NULL) {
		/* Parse built in configuration */
		memset(conf, 0, sizeof *conf);
		conf_parse(conf, config_defaults, sizeof config_defaults);
		replace_home(&(conf->_conf_file.val), &(conf->_conf_file.len));
		return 1;
	} else {
		size_t len;
		char *config_file;

		/* Read in the configuration file at path */
		file_into_buf(path, &config_file, &len);
		if (config_file && len > 0) {
			conf_parse(conf, config_file, len);
			free(config_file);
		} else {
			return -1;
		}
	}

	replace_home(&(conf->logs_file.val), &(conf->logs_file.len));
	replace_home(&(conf->save_file.val), &(conf->save_file.len));

	debug("Configuration Initialization complete.");

	return 1;
}

int conf_parse(struct config *conf, const char *str, size_t len)
{
	size_t i;

	if (str == NULL || len == 0)
		return 0;

	struct {
		char *key;
		int (*cb)(struct config *, const char *, size_t);
	} tokens[] = {
		{ "set", conf_command_set },
		{ "unset", conf_command_unset },
		{ "bind", conf_command_bind },
	};

	getnextline(NULL, 0, NULL);

	char *pbuf;
	while (getnextline(str, len, &pbuf) != EOF && (pbuf != NULL)) {

		for (i = 0; i < LEN(tokens); i++) {
			if (strstr(pbuf, tokens[i].key) != pbuf)
				continue;
			break;
		}

		if (i >= LEN(tokens))
			goto again;

		tokens[i].cb(conf, pbuf, strlen(pbuf));

	again:
		free(pbuf);
		continue;
	}

	return 1;
}

int conf_command_set(struct config *conf, const char *cmd, size_t len)
{
	size_t i;
	const char *pfirst, *plast;

	if (len < strlen("set") +2 || strstr(cmd, "set") != cmd)
		return 0;

	struct {
		char *key;
		struct values *val;
	} tokens_val[] = {
		{ "username", &conf->username },
		{ "logs_file", &conf->logs_file },
		{ "save_file", &conf->save_file },
		{ "_conf_file", &conf->_conf_file },
	};

	struct values *mod = NULL;
	pfirst = cmd;

	/* Find something like `set username ...` */
	for (i = 0; i < LEN(tokens_val); i++) {
		char *tmp;
		tmp = strstr(cmd, tokens_val[i].key);
		if (tmp == NULL)
			continue;

		mod = tokens_val[i].val;
		pfirst = tmp + strlen(tokens_val[i].key);
		break;
	}

	if (i >= LEN(tokens_val) || mod == NULL)
		return 0;

	/* Find first quotation or end of line */
	while (*pfirst != '"' && *pfirst != '\0')
		pfirst++;

	if (*pfirst != '"') {
		log_warn("Expected value of form: \"string\". Skipping.");
		return 0;
	}

	/* Find last quotation or end of line */
	plast = pfirst +1;
	while (*plast != '"' && *plast != '\0')
		plast++;

	if (*plast != '"') {
		log_warn("Expected value of form: \"string\". Skipping.");
		return 0;
	}

	//realloc here is used so we can parse the same option multiple times
	//without having to constantly recycle memory when the value changes.
	mod->len = (plast - pfirst);
	mod->val = realloc(mod->val, mod->len);

	if (mod->val == NULL) {
		log_err("Out of memory");
		return -1;
	}

	strncpy(mod->val, pfirst+1, mod->len -1);
	if (mod->len > 0)
		mod->val[mod->len -1] = '\0';

	return 1;
}

/* TODO */
int conf_command_unset(struct config *conf, const char *cmd, size_t len)
{
	if (!conf || !len || strstr(cmd, "unset") != cmd)
		return 0;

	return 0;
}

int conf_command_bind(struct config *conf, const char *cmd, size_t len)
{
	size_t i;
	const char *pchar;

	if (len < strlen("bind") +1 || strstr(cmd, "bind") != cmd)
		return 0;

	struct {
		char *key;
		struct key_bindings *kb;
	} tokens[] = {
		{ "move_drop", &conf->move_drop },
		{ "move_left", &conf->move_left },
		{ "move_right", &conf->move_right },
		{ "move_down", &conf->move_down },
		{ "rotate_left", &conf->rotate_left },
		{ "rotate_right", &conf->rotate_right },

		{ "hold_key", &conf->hold_key },
		{ "pause_key", &conf->pause_key },
		{ "quit_key", &conf->quit_key },

		{ "toggle_ghosts", &conf->toggle_ghosts },
		{ "toggle_wallkicks", &conf->toggle_wallkicks },
		{ "cycle_gamemodes", &conf->cycle_gamemodes },
		{ "talk_key", &conf->talk_key },
	};

	struct key_bindings *mod = NULL;
	pchar = cmd;

	for (i = 0; i < LEN(tokens); i++) {
		char *tmp;
		tmp = strstr(cmd, tokens[i].key);
		if (tmp == NULL)
			continue;

		mod = tokens[i].kb;
		pchar = tmp + strlen(tokens[i].key);
		break;
	}

	if (mod == NULL)
		return -1;

	/* Find first quotation or end of line */
	while (*pchar != '\'' && *pchar != '\0')
		pchar++;

	if (*pchar != '\'') {
		log_warn("Expected value of form: \'char\'. Skipping.");
		return 0;
	}

	/* char after first quotation mark */
	pchar++;

	/* Make sure input looks like: 'c' */
	if (pchar[1] != '\'') {
		log_warn("Expected value of form: \'char\'. Skipping.");
		return 0;
	}

	mod->enabled = true;
	mod->key = *pchar;

	return 1;
}

void conf_cleanup(struct config *conf)
{
	free(conf->username.val);
	free(conf->logs_file.val);
	free(conf->save_file.val);
	free(conf->_conf_file.val);
	free(conf);
}
