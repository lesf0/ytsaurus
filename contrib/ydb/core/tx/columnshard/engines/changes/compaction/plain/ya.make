LIBRARY()

SRCS(
    column_cursor.cpp
    column_portion_chunk.cpp
    merged_column.cpp
    GLOBAL logic.cpp
)

PEERDIR(
    contrib/ydb/core/tx/columnshard/engines/changes/compaction/common
)

END()
