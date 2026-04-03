"""
Sage Agent — Capability Registry Additions

Defines the allowed MCP tool set for the Sage (Gadget) agent.
This is intentionally restricted compared to Crunchy (authenticated) —
Sage can only read menus/items/inventory and create sales orders.

Integration: Merge these definitions into
  server-chatkit/app/agents/capability_registry.py
"""

from typing import FrozenSet

# Tools the Sage agent is allowed to call via MCP
GADGET_CAPABILITIES: FrozenSet[str] = frozenset({
    # Discovery tools (same as all agents)
    "get_user_profile",
    "get_service_info",
    "get_type_info",
    "make_api_request",

    # Public tool
    "google_place_insights",
})

# API paths the Sage agent is allowed to access via make_api_request.
# This acts as a secondary allowlist — even if the tool is available,
# Sage can only hit these endpoints.
GADGET_ALLOWED_PATHS: FrozenSet[str] = frozenset({
    # Read operations (GET)
    "GET /items",
    "GET /items/*",
    "GET /menus",
    "GET /menus/*",
    "GET /modifier-lists",
    "GET /modifier-lists/*",
    "GET /categories",
    "GET /categories/*",
    "GET /categories/tree",
    "GET /sales-orders",
    "GET /sales-orders/*",
    "GET /taxes",

    # Write operations (strictly limited)
    "POST /sales-orders",
})

# Explicitly blocked paths — these must never be accessible to Sage
# even if the allowlist above is accidentally expanded.
GADGET_BLOCKED_PATHS: FrozenSet[str] = frozenset({
    # Financial / analytics
    "GET /analytics/*",
    "POST /ai/tools/business_context",

    # Back-of-house operations
    "POST /purchase-orders",
    "PUT /purchase-orders/*",
    "DELETE /purchase-orders/*",
    "PUT /items/*",
    "DELETE /items/*",
    "POST /items",
    "PUT /menus/*",
    "DELETE /menus/*",

    # Inventory mutations
    "POST /stock-take/*",
    "PUT /stock-take/*",

    # Admin
    "GET /members",
    "POST /members",
    "PUT /members/*",
    "DELETE /members/*",
    "GET /organizations",
    "PUT /organizations/*",
})


def get_capability_registry_additions() -> dict:
    """
    Returns the additions needed for capability_registry.py.

    Usage in capability_registry.py:
        from sage_capabilities import GADGET_CAPABILITIES

        _CAPABILITY_MAP["gadget"] = GADGET_CAPABILITIES
    """
    return {
        "gadget": GADGET_CAPABILITIES,
    }
