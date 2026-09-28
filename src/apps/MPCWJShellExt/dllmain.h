/*
 * (C) 2012-2018 see Authors.txt
 *
 * This file is part of MPC-WJ.
 *
 * MPC-WJ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-WJ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

class CMPCWJShellExtModule : public CAtlDllModuleT< CMPCWJShellExtModule >
{
public :
	DECLARE_LIBID(LIBID_MPCWJShellExtLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MPCWJSHELLEXT, "{631B91DB-D412-4947-8E5A-340E4FF7E1BD}")
};

extern class CMPCWJShellExtModule _AtlModule;
