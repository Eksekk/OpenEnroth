cmake_minimum_required(VERSION 3.20.4 FATAL_ERROR)

set(ENGINE_TABLES_SOURCES
        ItemTable.cpp
        FactionTable.cpp
        FrameTableInc.cpp
        IconFrameTable.cpp
        PlayerFrameTable.cpp
        StorylineTextTable.cpp
        TileTable.cpp
        BuildingTable.cpp
        AwardTable.cpp
        AutonoteTable.cpp
        QuestTable.cpp
        MessageScrollTable.cpp)

set(ENGINE_TABLES_HEADERS
        ItemTable.h
        FactionTable.h
        FrameTableInc.h
        IconFrameTable.h
        PlayerFrameTable.h
        StorylineTextTable.h
        TileFrameTable.h
        BuildingTable.h
        AwardTable.h
        AutonoteTable.h
        QuestTable.h
        MessageScrollTable.h)

add_library(engine_tables STATIC ${ENGINE_TABLES_SOURCES} ${ENGINE_TABLES_HEADERS})
target_link_libraries(engine_tables engine)
target_check_style(engine_tables)
