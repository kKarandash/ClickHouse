#pragma once

#include <Core/Names.h>
#include <Core/Block.h>
#include <Interpreters/Context_fwd.h>


namespace DB
{

class IFunctionOverloadResolver;
using FunctionOverloadResolverPtr = std::shared_ptr<IFunctionOverloadResolver>;

class DataTypeArray;
class ColumnArray;

const DataTypeArray * getArrayJoinDataType(const DataTypePtr & type, bool allow_map = true);
const ColumnArray * getArrayJoinColumn(const ColumnPtr & column, bool allow_map = true);


class ArrayJoinAction
{
public:
    NameSet columns;
    bool is_left = false;
    bool is_unaligned = false;

    /// For unaligned [LEFT] ARRAY JOIN
    FunctionOverloadResolverPtr function_length;
    FunctionOverloadResolverPtr function_greatest;
    FunctionOverloadResolverPtr function_array_resize;

    /// For LEFT ARRAY JOIN.
    FunctionOverloadResolverPtr function_builder;

    ArrayJoinAction(const NameSet & array_joined_columns_, bool array_join_is_left, ContextPtr context);
    void prepare(ColumnsWithTypeAndName & sample) const;
    void execute(Block & block);
};

using ArrayJoinActionPtr = std::shared_ptr<ArrayJoinAction>;

}
