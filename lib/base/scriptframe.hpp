/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2016 Icinga Development Team (https://www.icinga.org/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#ifndef SCRIPTFRAME_H
#define SCRIPTFRAME_H

#include "base/i2-base.hpp"
#include "base/dictionary.hpp"
#include "base/array.hpp"
#include <boost/thread/tss.hpp>
#include <stack>

namespace icinga
{

struct I2_BASE_API ScriptFrame
{
	ScriptFrame(void);
	ScriptFrame(const Value& self);
	~ScriptFrame(void);

	static void StaticInitialize(void);

	void IncreaseStackDepth(void);
	void DecreaseStackDepth(void);

	static ScriptFrame *GetCurrentFrame(void);

	static Array::Ptr GetImports(void);
	static void AddImport(const Object::Ptr& import);

	bool HasLocals(void) const;

	Dictionary::Ptr& GetLocals(void);
	void SetLocals(const Dictionary::Ptr& locals);

	Value& GetSelf(void);
	void SetSelf(const Value& self);

	bool IsSandboxed(void) const;
	void SetSandboxed(bool sandboxed);

private:
	Dictionary::Ptr m_Locals;
	Value m_Self;
	bool m_Sandboxed;
	int m_Depth;
	bool m_InitLocals;

	static boost::thread_specific_ptr<std::stack<ScriptFrame *> > m_ScriptFrames;
	static Array::Ptr m_Imports;

	inline static void PushFrame(ScriptFrame *frame);
	inline static ScriptFrame *PopFrame(void);

	void InitializeFrame(void);
};

}

#endif /* SCRIPTFRAME_H */
