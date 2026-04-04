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
#include <widecharwidth/widechar_width.h>

#include <utfcpp/source/utf8/checked.h>

#include "common.h"

std::string mks(const String32 &txt)
{
	std::string rval;
	try {
		utf8::utf32to8(txt.begin(), txt.end(), std::back_inserter(rval));
	} catch (utf8::invalid_utf8 &e)
	{

	}
	return rval;
}

String32 mkws(const std::string &txt)
{
	String32 rval;

	try {
		utf8::utf8to32(txt.begin(), txt.end(), std::back_inserter(rval));
	} catch (utf8::invalid_utf8 &e)
	{

	}

	return rval;
}

int real_wcwidth(char32_t ch)
{
        int width = widechar_wcwidth(ch);
        switch (width)
        {
        case widechar_nonprint:
                return 0;
        case widechar_combining:
                return 0;
        case widechar_ambiguous:
                return 1;
        case widechar_private_use:
                return 1;
        case widechar_unassigned:
                return 1;
        case widechar_widened_in_9:
                return 2;
        case widechar_non_character:
                return 0;
        }
        return width;
}
