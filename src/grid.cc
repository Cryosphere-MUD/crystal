/*
 * Crystal Mud Client
 * Copyright (C) 2002-2025 Abigail Brady
 * Copyright (C) 2002 Owen Cliffe
 * Copyright (C) 2004 Stuart Brady
 * Copyright (C) 2004 Paul Lettington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#define _XOPEN_SOURCE

#include "grid.h"
#include "common.h"
#include "telnet.h"

#include <ctype.h>
#include <time.h>
#include <wchar.h>
#include <sstream>

extern mterm tty;

enum
{
	OSC_ESCAPE = 256,
};

void grid_t::show_batch(const cellstring &batch)
{
	for (int i = 0; i < batch.length(); i++)
		place(&batch[i]);
}

void grid_t::place(const cell_t *ri)
{
	cell_t fi = *ri;
	cell_t *i = &fi;

	if (conn->lp_prompts)
	{
		if (lastprompt)
		{
			cstoredprompt.erase();
			for (int i = 0; i < col; i++)
				cstoredprompt += get(row, i);
			col = 0;
			eraseline(0);
			lastprompt = 0;
		}
	}

	for (int i = 0; i < nlw; i++)
		newline();
	nlw = 0;

	if (i->ch == '\r')
		return;
	if (i->ch == '\t')
	{
		set(row, col, myblank());
		col++;
		while ((col & 7) && col < tty.WIDTH)
		{
			set(row, col, myblank());
			col++;
		}
		return;
	}

	if (col >= tty.WIDTH || i->ch == '\n')
	{
		newline();
		if (i->ch == '\n')
			return;
	}

	if (((i->ch < 0x20) || (i->ch >= 0x80 && i->ch <= 0x9f)))
		i->ch = 0x241b;

	set(row, col, *i);
	col++;
	if (wcwidth(i->ch) == 2)
	{
		set(row, col, cell_t(-i->ch));
		col++;
	}
}

std::string q(int ch)
{
	if (ch == ESC)
		return "\\e";
	if (ch == CR)
		return "\\r";
	if (ch == CSI)
		return "\\e[";
	if (ch == BS)
		return "\\b";
	if (ch == LF)
		return "\\n";
	if (ch == BEL)
		return "\\a";
	if (ch == 0)
		return "\\0";
	char a[2] = {char(ch), 0};
	return a;
}

void grid_t::osc_end()
{
	if (osc_string.substr(0, 2) == "0;" || osc_string.substr(0, 2) == "2;")
	{
		std::string new_title = osc_string.substr(2);
		tty.title(_("%s - Crystal"), new_title.c_str());
	}
	osc_string = "";
}

static int parse_truecol(std::vector<int>::iterator &parit, const std::vector<int>::iterator end, int def)
{
	int comps[3];
	for (int i = 0; i < 3; i++)
	{
		if (parit == end)
			return def;
		comps[i] = *parit;
		parit++;
	}
	return make_truecol(comps[0], comps[1], comps[2]);
}

int last_mode = -1;

std::vector<int> parse(const std::string& s) {
    std::vector<int> result;
    std::stringstream ss(s);
    std::string token;

    while (std::getline(ss, token, ';'))
        result.push_back(std::stoi(token));

    return result;
}

void grid_t::wterminal(wchar_t ch)
{
	// if (last_mode != mode)
	// {
	// 	fprintf(stderr, "\nmode: %3s, got:%s.\n", q(mode).c_str(), q(ch).c_str());
	// 	last_mode = mode;
	// }
	// else
	// 	fprintf(stderr, "%s", q(ch).c_str());

	if (mode == 0)
	{
		switch (ch)
		{
		case 0:
			return;

		case CR:
			col = 0;
			changed = 1;
			return;

		case BS:
			col--;
			if (col < 0)
				col = 0;
			changed = 1;
			return;

		case BEL:
			tty.beep();
			return;

		case SI:
			scs = 1;
			return;

		case SO:
			scs = 0;
			return;

		case ESC:
			mode = ESC;
			param_string = "";
			return;

		case CSI:
			mode = CSI;
			param_string = "";
			return;

		default:

			if (ch >= 0x80 && ch <= 0x9f)
				ch = '?';

			if (ch == LF || ch == FF || ch == VT)
				ch = LF;

			if (hidden)
				ch = ' ';

			int scs = this->scs;
			if (scs && (ch < '`' || ch > '~'))
				scs = 0;

			cell_t a = cell_t(ch, inten, forecol, backcol, scs, ul, it, fr, os, inv, ol);
			place(&a);

			changed = 1;
		}
		return;
	}

	if (mode == ESC)
	{
		if (ch == '[')
		{
			mode = CSI;
			return;
		}
		if (ch == ']')
		{
			mode = ']';
			return;
		}
		if (ch == '(')
		{
			mode = '(';
			return;
		}
		if (ch == '%')
		{
			mode = '%';
			return;
		}
		if (ch == '-')
		{
			mode = '-';
			return;
		}
		if (ch >= 48 && ch <= 126)
		{
			mode = 0;
			return;
		}
	}

	if (mode == '%')
	{
		if (ch == '@')
			conn->mud_cset = "ISO-8859-1";

		if (ch == 'G')
			conn->mud_cset = "UTF-8";

		mode = 0;

		return;
	}

	if (mode == '-')
	{
		if (ch == 'A')
			conn->mud_cset = "ISO-8859-1";
		if (ch == 'B')
			conn->mud_cset = "ISO-8859-2";

		mode = 0;
		return;
	}

	if (mode == '(')
	{
		mode = 0;
		if (ch == 'B')
			scs = 0;
		if (ch == '0')
			scs = 1;
		return;
	}

	if (mode == OSC)
	{
		if (ch == '\a' || ch == ST)
		{
			mode = 0;
			osc_end();
			return;
		}
		if (ch == '\e')
		{
			mode = OSC_ESCAPE;
			return;
		}
		osc_string += ch;
		return;
	}

	if (mode == OSC_ESCAPE)
	{
		if (ch == '\\')
		{
			mode = 0;
			osc_end();
		}
		return;
	}

	if (mode == CSI)
	{

		if (isdigit(ch) || ch == ';')
		{
			param_string += ch;
			return;
		}

		auto params = parse(param_string);

		if (ch == 'A')
		{ /* CUU - CUrsor Up */
			if (params.size())
				row -= params[0];
			else
				row--;
			if (row < 0)
				row = 0;
			mode = 0;
			changed = 1;
			return;
		}

                /* 'B' CUD : CUrsor Down */

                if (ch == 'C')
		{ /* CUF - cursor forward  */
			if (params.size())
				col += params[0];
			else
				col++;
			if (col >= tty.WIDTH)
				col = tty.WIDTH - 1;
			mode = 0;
			changed = 1;
			return;
		}

		if (ch == 'D')
		{ /* CUB - cursor back */
			if (params.size())
				col -= params[0];
			else
				col--;
			if (col < 0)
				col = 0;
			mode = 0;
			changed = 1;
			return;
		}

                /* CHA - cursor horizontal absolute */
                if (ch == 'G')
		{
                        if (params.size())
			    col = params[0] - 1;
                        else
                            col = 0;
			mode = 0;
			changed = 1;
			return;
		}

                /* CUP - cursor position (only home implemented) */
                if (ch == 'H')
		{
			if (conn->grid != this)
			{
				row = 0;
				col = 0;
				mode = 0;
				changed = 1;
			}
			mode = 0;
			return;
		}

		 /* EL - Erase in Line */
                if (ch == 'K')
		{
			if (params.empty())
				params.push_back(0);

			/* 0 - default - cursor to end of line */
			/* 1 -           start to cursor (including) */
			/* 2 -           all of line */

			int par = params[0];
			if (par == 0)
				eraseline(col);
			else if (par == 2)
				eraseline(0);
		}

		if (ch == 'n') /* DEVICE STATUS REPORT */
		{
			if (params.size() == 1 && params[0] == 6)
			{
				/* request of cursor position */
				char blah[1024];
				sprintf(blah, "\033[%i;%iR", row, col);
				conn->telnet->send(blah);
			}
		}

		if (ch == 'm')
		{ /* SGR - Select Graphics Rendition */
			if (params.empty())
				params.push_back(0);

			auto parit = params.begin();
			while (parit != params.end())
			{
				int parm = *parit;
				parit++;
				switch (parm)
				{
				case 0:
					inten = I_NORM;
					forecol = deffc;
					backcol = defbc;
					ul = 0;
					it = 0;
					fr = 0;
					inv = 0;
					hidden = 0;
					os = 0;
					ol = 0;
					break;
				case 1:
					inten = I_BOLD;
					break;
				case 2:
					inten = I_DIM;
					break;
				case 3:
					it = 1;
					fr = 0;
					break;
				case 4:
					ul = 1;
					break;
				case 7:
					inv = 1;
					break;
				case 8:
					hidden = 1;
					break;
				case 9:
					os = 1;
					break;
				case 20:
					fr = 1;
					it = 0;
					break;
				case 21:
					ul = 2;
					break;
				case 22:
					inten = I_NORM;
					break;
				case 23:
					fr = it = 0;
					break;
				case 24:
					ul = 0;
					break;
				case 27:
					inv = 0;
					break;
				case 28:
					hidden = 0;
					break;
				case 29:
					os = 0;
					break;
				case 30 ... 37:
					forecol = parm - 30;
					break;
				case 39:
					forecol = deffc;
					break;
				case 38:
					if (parit != params.end())
					{
						if (*parit == 5)
						{
							parit++;
							if (parit != params.end())
							{
								forecol = *parit;
								parit++;
								continue;
							}
						}
						if (*parit == 2)
						{
							parit++;
							forecol = parse_truecol(parit, params.end(), 7);
							continue;
						}
					}
					break;
				case 40 ... 47:
					backcol = parm - 40;
					break;
				case 49:
					backcol = defbc;
					break;
				case 48:
					if (parit != params.end())
					{
						if (*parit == 5)
						{
							parit++;
							if (parit != params.end())
							{
								backcol = *parit;
								parit++;
								continue;
							}
						}
						if (*parit == 2)
						{
							parit++;
							backcol = parse_truecol(parit, params.end(), 0);
							continue;
						}
					}
					break;
				case 53:
					ol = 1;
					break;
				case 55:
					ol = 0;
					break;
				}
			}

			mode = 0;
			param_string.clear();
			return;
		}

		if (ch >= 0x30 && ch <= 0x7e)
		{
			// infof("unrecognised ANSI command %c", ch);
			mode = 0;
			param_string.clear();
		}

		return;
	}
}

bool info_to_stderr = true;

void grid_t::infof(const char *fmt, ...)
{
	char buf[10000];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	info(buf);
}

void grid_t::infoc(wchar_t w)
{
	if (info_to_stderr)
		fprintf(stderr, "%lc", w);

	static my_wstring isf = L"";

	isf += w;

	if (w == '\n')
	{
		cellstring q;
		int c = -1;
		int p = lastprompt;
		lastprompt = 0;
		if (col)
		{
			for (int i = 0; i < get_len(row); i++)
				q += get(row, i);
			c = col;
			col = 0;
			eraseline(0);
		}
		for (size_t i = 0; i < isf.length(); i++)
		{
			cell_t c = cell_t(isf[i]);
			place(&c);
		}

		if (c != -1)
		{
			for (int i = 0; i < q.length(); i++)
				place(&q[i]);
			c = col;
		}
		lastprompt = p;
		changed = 1;
		isf = L"";
		return;
	}
}

void grid_t::info(const char *str)
{
	int max = strlen(str);
	while (1)
	{
		wchar_t c;
		int s = mbtowc(&c, str, max);
		if (s <= 0)
			return;
		infoc(c);
		str += s;
		max -= s;
	}
}

void grid_t::info(const wchar_t *w)
{
	while (*w)
	{
		infoc(*w);
		w++;
	}
}

void grid_t::info(const my_wstring &str)
{
	for (size_t i = 0; i < str.length(); i++)
		infoc(str[i]);
}

bool grid_t::file_dump(const char *file)
{
	FILE *dumpfile = fopen(file, "a");
	if (NULL == dumpfile)
	{
		infof(_("/// couldn't open '%s' to dump to\n"), file);
		return 0;
	}
	infof(_("/// dumping scroll history to '%s'\n"), file);

	time_t cur_time = time(NULL);
	std::string ctime_str = ctime(&cur_time);
	ctime_str = ctime_str.substr(0, ctime_str.length() - 1);

	fprintf(dumpfile, _("---=== Dump generated on %s by crystal ===---\n"), ctime_str.c_str());
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < get_len(i); j++)
			fprintf(dumpfile, "%lc", get(i, j).ch);
		fprintf(dumpfile, "\n");
	}
	fprintf(dumpfile, _("---=== End of dump ===---\n"));
	fclose(dumpfile);
	return true;
}
