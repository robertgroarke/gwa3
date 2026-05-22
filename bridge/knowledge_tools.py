"""Static farming-knowledge tool dispatch."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

from . import farming_knowledge


KnowledgeLookup = Callable[[dict[str, Any]], dict]


KNOWLEDGE_TOOL_HANDLERS: dict[str, KnowledgeLookup] = {
    "get_recipe": lambda args: farming_knowledge.get_recipe(args.get("consumable_model_id", 0)),
    "get_outpost_info": lambda args: farming_knowledge.get_outpost_info(args.get("map_id", 0)),
    "get_material_info": lambda args: farming_knowledge.get_material_info(args.get("model_id", 0)),
    "get_dungeon_info": lambda args: farming_knowledge.get_dungeon_info(args.get("name", "")),
    "get_blessing_info": lambda args: farming_knowledge.get_blessing_info(args.get("blessing_type", "")),
    "get_hero_build": lambda args: farming_knowledge.get_hero_build(args.get("hero_name", "")),
    "get_quest_info": lambda args: farming_knowledge.get_quest_info(args.get("key", "")),
}
KNOWLEDGE_TOOL_NAMES = frozenset(KNOWLEDGE_TOOL_HANDLERS)


def execute_knowledge_tool(name: str, params: dict[str, Any]) -> dict | None:
    handler = KNOWLEDGE_TOOL_HANDLERS.get(name)
    if handler is None:
        return None
    return handler(params)


def knowledge_tool_result(data: dict) -> dict:
    return {
        "success": "error" not in data,
        "data": data,
        "error": data.get("error"),
    }
