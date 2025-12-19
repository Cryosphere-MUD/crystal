/* 
 * Crystal Mud Client
 * Copyright (C) Abigail Brady, Paul Lettington, Owen Cliffe, Stuart Brady
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

#ifndef GRID_H
#define GRID_H

#include "common.h"
#include "crystal.h"
#include "io.h"
#include "scripting.h"

#include <list>
#include <vector>

struct ansi_context
{
	// the escape mode
	int mode;

	// parameters so far
	std::list<int> pars;

	// current paremeter
	int par;

	std::string osc_string;

	Intensity inten;
	int forecol, backcol;
	bool scs, it, fr, inv, os, hidden, ol;
	int ul : 2;
	int defbc, deffc;

	void init()
	{
		mode = 0;
		pars = std::list<int>();
		par = 0;
		inten = I_NORM;
		deffc = forecol = COL_DEFAULT;
		defbc = backcol = COL_DEFAULT;
		scs = 0;
		ul = 0;
		it = 0;
		fr = 0;
		inv = 0;
		os = 0;
		ol = 0;
		hidden = 0;
	}
};

typedef std::vector<cell_t> line_t;

#define GRIDHEIGHT 10000

class grid_t : public ansi_context
{
      public:
	conn_t *conn;

	std::vector<line_t> lines;
	int first;
	int n;
	int lcount;

	int row;
	int col;

	int lastprompt;

	int height;
	int width;

	int visible;

	int changed;

	cellstring cstoredprompt;

	void set_conn(conn_t *c) { conn = c; }

	grid_t(conn_t *c)
	{
		conn = 0;
		first = 0;
		n = 0;
		lcount = GRIDHEIGHT;
		lines.reserve(lcount);
		lastprompt = 0;
		nlw = 0;
		row = 0;
		col = 0;
		visible = 1;
		changed = 0;

		height = 0;
		width = 0;

		init();
	}

	cell_t myblank()
	{
		cell_t b = blank;
		b.fc = deffc;
		b.bc = defbc;
		return b;
	}

	cell_t get(int r, int c)
	{
		if (r < first)
			return myblank();
		if (r > (first + n))
			return myblank();

		if (c < 0 || c >= lines[r % lcount].size())
			return myblank();

		return lines[r % lcount][c];
	}

	void newline()
	{
		my_wstring s;
		for (int i = 0; i < get_len(row); i++)
			s += get(row, i).ch;

		if (conn->logfile)
		{
			for (int i = 0; i < get_len(row); i++)
				fprintf(conn->logfile, "%lc", get(row, i).ch);
			fprintf(conn->logfile, "\n");
			fflush(conn->logfile);
		}

		row++;
		col = 0;
		n++;

		lines[row % lcount].clear();

		if (n > lcount)
		{
			first++;
			n--;
		}
		scripting::dotrigger(s);
	}

	void eraseline(int from)
	{
		lines[row % lcount].resize(from);
	}

	int get_len(int r)
	{
		if (r < first)
			return 0;
		if (r > (first + n))
			return 0;

		return lines[r % lcount].size();
	}

	void set(int r, int c, cell_t ch)
	{
		if (c >= 0 && c < MAXWIDTH)
		{
			lines[r % lcount].resize(std::max(size_t(c + 1), lines[r % lcount].size()));
			lines[r % lcount][c] = ch;
		}
	}

	int nlw;

	void wantnewline() { nlw++; }

	void place(const cell_t *ri);

	void osc_end();

	void wterminal(wchar_t ch);

	void show_batch(const cellstring &batch);

	bool file_dump(const char *file);

	// the info functions place characters directly on the grid ignoring the
	// current ANSI formatting.  These should be used to insert client-originated
	// messages into the stream.
	void info(const char *);
	void info(const wchar_t *);
	void info(const my_wstring &);

	void infof(const char *fmt, ...) /* __attribute__ (( format (printf, 2, 3) )) */;

      private:
	void infoc(wchar_t w);
};

inline void have_prompt(grid_t *grid)
{
	grid->lastprompt = 1;

	my_wstring s;
	for (int i = 0; i < grid->get_len(grid->row); i++)
		s += grid->get(grid->row, i).ch;
	scripting::doprompt(s);
}

extern int info_to_stderr;

#endif
