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

#ifndef JITOPS_H
#define JITOPS_H

#include "config/i2-config.hpp"
#include "config/expression.hpp"
#include "config/configitembuilder.hpp"
#include "config/applyrule.hpp"
#include "config/objectrule.hpp"
#include "base/debuginfo.hpp"
#include "base/array.hpp"
#include "base/dictionary.hpp"
#include "base/function.hpp"
#include "base/scriptglobal.hpp"
#include "base/exception.hpp"
#include "base/convert.hpp"
#include "base/objectlock.hpp"
#include <boost/foreach.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <map>
#include <vector>

namespace icinga
{

inline void JitNewValueEmpty(Value *res)
{
	*res = Empty;
}

inline void JitNewValueNumber(double val, Value *res)
{
	new (res) Value(val);
}

inline void JitNewValueBoolean(int val, Value *res)
{
	new (res) Value(static_cast<bool>(val));
}

inline void JitNewValueString(const char *val, size_t len, Value *res)
{
	new (res) Value(String(val, val + len));
}

inline void JitNewValueObject(Object *val, Value *res)
{
	new (res) Value(val);
}

inline void FreeValue(Value *val)
{
	delete val;
}

inline void JitInvokeDoEvaluate(Expression *expr, ScriptFrame *frame, DebugHint *dhint, Value *res)
{
	new (res) Value(expr->DoEvaluate(*frame, dhint));
}

inline void EmitJitInvokeDoEvaluate(asmjit::X86Compiler& compiler, Expression *expr,
	asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitInvokeDoEvaluate),
		asmjit::FuncBuilder4<void, Expression *, ScriptFrame *, DebugHint *, ExpressionResult *>(asmjit::kCallConvHost));
	call->setArg(0, asmjit::imm_ptr(expr));
	call->setArg(1, frame);
	call->setArg(2, dhint);
	call->setArg(3, res);
}

inline void JitDeleteExpression(Expression *expr)
{
	delete expr;
}

inline void EmitJitDeleteExpression(asmjit::X86Compiler& compiler, Expression *expr)
{
	asmjit::X86CallNode *dcall = compiler.addCall(asmjit::imm_ptr(&JitDeleteExpression), asmjit::FuncBuilder1<void, Expression *>(asmjit::kCallConvHost));
	dcall->setArg(0, asmjit::imm_ptr(expr));
}

inline void EmitJitNewValue(asmjit::X86Compiler& compiler, const Value& value, asmjit::X86GpVar& res)
{
	asmjit::X86CallNode *call;

	switch (value.GetType()) {
		case ValueEmpty:
		{
			call = compiler.addCall(asmjit::imm_ptr(&JitNewValueEmpty), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
			call->setArg(0, res);

			break;
		}

		case ValueNumber:
		{
			double num = value;
			asmjit::X86Mem val = compiler.newDoubleConst(asmjit::kConstScopeLocal, value);

			asmjit::X86XmmVar pval = compiler.newXmmSd("pval");
			compiler.movsd(pval, val);

			call = compiler.addCall(asmjit::imm_ptr(&JitNewValueNumber), asmjit::FuncBuilder2<void, double *, Value *>(asmjit::kCallConvHost));
			call->setArg(0, pval);
			call->setArg(1, res);
			break;
		}

		case ValueBoolean:
		{
			call = compiler.addCall(asmjit::imm_ptr(&JitNewValueBoolean), asmjit::FuncBuilder2<void, int, Value *>(asmjit::kCallConvHost));
			call->setArg(0, asmjit::imm(static_cast<bool>(value)));
			call->setArg(1, res);

			break;
		}

		case ValueString:
		{
			String str = value;

			asmjit::X86Mem val = compiler.newConst(asmjit::kConstScopeLocal, str.CStr(), str.GetLength() + 1);

			asmjit::X86GpVar pval = compiler.newIntPtr("pval");
			compiler.lea(pval, val);

			call = compiler.addCall(asmjit::imm_ptr(&JitNewValueString), asmjit::FuncBuilder3<void, const char *, size_t, Value *>(asmjit::kCallConvHost));
			call->setArg(0, pval);
			call->setArg(1, asmjit::imm(str.GetLength() + 1));
			call->setArg(1, res);

			break;
		}

		case ValueObject:
		default:
			VERIFY(!"Unsupported Value type for JIT.");
	}
}

}

#endif /* JITOPS_H */
