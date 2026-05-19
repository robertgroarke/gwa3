"""Bridge-side player trade safety checks."""

from dataclasses import dataclass, field
from typing import Any

from .config import (
    TRADE_ACCEPT_VALUE_TOLERANCE,
    TRADE_HARD_REFUSAL_TERMS,
    TRADE_WHISPER_REFUSAL_TERMS,
)


def _trade_error(reason: str, **detail: Any) -> dict:
    return {
        "success": False,
        "error": f"trade_unsafe:{reason}",
        "trade_guard": detail,
    }


def _trade(snapshot: dict | None) -> dict:
    return ((snapshot or {}).get("trade", {}) or {})


def _party(snapshot: dict | None, side: str) -> dict:
    return (_trade(snapshot).get(side, {}) or {})


def _item_key(item: dict) -> tuple:
    return (
        int(item.get("item_id", 0) or 0),
        int(item.get("model_id", 0) or 0),
        int(item.get("quantity", 0) or 0),
        int(item.get("value", 0) or 0),
    )


def _party_signature(party: dict) -> tuple:
    items = tuple(sorted(_item_key(item) for item in party.get("items", []) or []))
    return (int(party.get("gold", 0) or 0), items)


def _item_text(item: dict) -> str:
    parts = [
        item.get("name", ""),
        item.get("full_name", ""),
        item.get("info_string", ""),
        item.get("customization", ""),
        item.get("inscription", ""),
    ]
    return " ".join(str(part) for part in parts if part).lower()


@dataclass
class TradeGuard:
    value_tolerance: float = TRADE_ACCEPT_VALUE_TOLERANCE
    hard_refusal_terms: tuple[str, ...] = TRADE_HARD_REFUSAL_TERMS
    whisper_refusal_terms: tuple[str, ...] = TRADE_WHISPER_REFUSAL_TERMS
    kamadan_medians: dict[int, int] = field(default_factory=dict)
    _last_partner_signature: tuple | None = None

    def record_submit_offer(self, snapshot: dict | None) -> None:
        self._last_partner_signature = _party_signature(_party(snapshot, "partner"))

    def evaluate_accept_trade(self, snapshot: dict | None) -> dict | None:
        trade = _trade(snapshot)
        if not trade.get("is_open"):
            return _trade_error("trade_window_closed")

        hard_refusal = self._find_hard_refusal(_party(snapshot, "player").get("items", []) or [])
        if hard_refusal:
            return _trade_error("hard_refusal_item", matched=hard_refusal)

        current_partner = _party_signature(_party(snapshot, "partner"))
        if self._last_partner_signature is None:
            return _trade_error("no_submitted_offer_baseline")
        if current_partner != self._last_partner_signature:
            return _trade_error(
                "partner_offer_changed",
                expected=self._last_partner_signature,
                actual=current_partner,
            )

        player_value = self._party_value(_party(snapshot, "player"))
        partner_value = self._party_value(_party(snapshot, "partner"))
        if player_value > self.value_tolerance * partner_value:
            return _trade_error(
                "value_imbalance",
                player_value=player_value,
                partner_value=partner_value,
                tolerance=self.value_tolerance,
            )
        return None

    def evaluate_whisper(self, message: str) -> dict | None:
        text = (message or "").lower()
        for term in self.whisper_refusal_terms:
            if term in text:
                return _trade_error("whisper_refused", matched=term)
        return None

    def _party_value(self, party: dict) -> int:
        total = int(party.get("gold", 0) or 0)
        for item in party.get("items", []) or []:
            quantity = max(1, int(item.get("quantity", 1) or 1))
            model_id = int(item.get("model_id", 0) or 0)
            median = int(self.kamadan_medians.get(model_id, 0) or 0)
            fallback = int(item.get("value", 0) or 0)
            total += quantity * (median if median > 0 else fallback)
        return total

    def _find_hard_refusal(self, items: list[dict]) -> str | None:
        for item in items:
            text = _item_text(item)
            for term in self.hard_refusal_terms:
                if term in text:
                    return term
        return None
