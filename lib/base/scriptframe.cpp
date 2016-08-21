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

#include "base/scriptframe.hpp"
#include "base/scriptglobal.hpp"
#include "base/exception.hpp"

using namespace icinga;

boost::thread_specific_ptr<std::stack<ScriptFrame *> > ScriptFrame::m_ScriptFrames;
Array::Ptr ScriptFrame::m_Imports;

INITIALIZE_ONCE_WITH_PRIORITY(&ScriptFrame::StaticInitialize, 50);

void ScriptFrame::StaticInitialize(void)
{
	Dictionary::Ptr systemNS = new Dictionary();
	ScriptGlobal::Set("System", systemNS);
	AddImport(systemNS);

	Dictionary::Ptr typesNS = new Dictionary();
	ScriptGlobal::Set("Types", typesNS);
	AddImport(typesNS);

	Dictionary::Ptr deprecatedNS = new Dictionary();
	ScriptGlobal::Set("Deprecated", deprecatedNS);
	AddImport(deprecatedNS);
}

ScriptFrame::ScriptFrame(void)
	: m_Self(ScriptGlobal::GetGlobals()), m_Sandboxed(false), m_Depth(0), m_InitLocals(true)
{
	InitializeFrame();
}

ScriptFrame::ScriptFrame(const Value& self)
	: m_Self(self), m_Sandboxed(false), m_Depth(0), m_InitLocals(true)
{
	InitializeFrame();
}

void ScriptFrame::InitializeFrame(void)
{
	std::stack<ScriptFrame *> *frames = m_ScriptFrames.get();

	if (frames && !frames->empty()) {
		ScriptFrame *frame = frames->top();

		m_Sandboxed = frame->m_Sandboxed;
	}

	PushFrame(this);
}

ScriptFrame::~ScriptFrame(void)
{
	ScriptFrame *frame = PopFrame();
	ASSERT(frame == this);
}

void ScriptFrame::IncreaseStackDepth(void)
{
	if (m_Depth + 1 > 300)
		BOOST_THROW_EXCEPTION(ScriptError("Stack overflow while evaluating expression: Recursion level too deep."));

	m_Depth++;
}

void ScriptFrame::DecreaseStackDepth(void)
{
	m_Depth--;
}

ScriptFrame *ScriptFrame::GetCurrentFrame(void)
{
	std::stack<ScriptFrame *> *frames = m_ScriptFrames.get();

	ASSERT(!frames->empty());
	return frames->top();
}

ScriptFrame *ScriptFrame::PopFrame(void)
{
	std::stack<ScriptFrame *> *frames = m_ScriptFrames.get();

	ASSERT(!frames->empty());

	ScriptFrame *frame = frames->top();
	frames->pop();

	return frame;
}

void ScriptFrame::PushFrame(ScriptFrame *frame)
{
	std::stack<ScriptFrame *> *frames = m_ScriptFrames.get();

	if (!frames) {
		frames = new std::stack<ScriptFrame *>();
		m_ScriptFrames.reset(frames);
	}

	if (!frames->empty()) {
		ScriptFrame *parent = frames->top();
		frame->m_Depth += parent->m_Depth;
	}

	frames->push(frame);
}

Array::Ptr ScriptFrame::GetImports(void)
{
	return m_Imports;
}

void ScriptFrame::AddImport(const Object::Ptr& import)
{
	Array::Ptr imports;

	if (!m_Imports)
		imports = new Array();
	else
		imports = m_Imports->ShallowClone();

	imports->Add(import);

	m_Imports = imports;
}

bool ScriptFrame::HasLocals(void) const
{
	return m_Locals != NULL;
}

Dictionary::Ptr& ScriptFrame::GetLocals(void)
{
	if (m_InitLocals) {
		m_Locals = new Dictionary();
		m_InitLocals = false;
	}

	return m_Locals;
}

void ScriptFrame::SetLocals(const Dictionary::Ptr& locals)
{
	m_Locals = locals;
	m_InitLocals = false;
}

Value& ScriptFrame::GetSelf(void)
{
	return m_Self;
}

void ScriptFrame::SetSelf(const Value& self)
{
	m_Self = self;
}

bool ScriptFrame::IsSandboxed(void) const
{
	return m_Sandboxed;
}

void ScriptFrame::SetSandboxed(bool sandboxed)
{
	m_Sandboxed = sandboxed;
}
