# Sage Agent — Integration Guide

Sage is the front-of-house AI persona for Crunch Gadget. These files are designed to be integrated into `server-chatkit` alongside the existing Crunchy and Alex agents.

## Files

| File | Purpose | Destination |
|------|---------|-------------|
| `sage_instructions.py` | System prompt — voice-optimised, FOH-scoped | `server-chatkit/app/agents/sage_instructions.py` |
| `sage_capabilities.py` | MCP capability allowlist — read + create orders only | Merge into `server-chatkit/app/agents/capability_registry.py` |

## Integration Steps

### 1. Copy the instruction file

```bash
cp agent/sage_instructions.py ../server-chatkit/app/agents/sage_instructions.py
```

### 2. Add to capability registry

In `server-chatkit/app/agents/capability_registry.py`, add:

```python
from app.agents.sage_capabilities import GADGET_CAPABILITIES

# Add to _CAPABILITY_MAP
_CAPABILITY_MAP["gadget"] = GADGET_CAPABILITIES
```

### 3. Add agent factory function

In `server-chatkit/app/agents/sage_instructions.py`, add:

```python
from agents import Agent, ModelSettings
from agents.mcp import MCPServerStreamableHttp

AGENT_MODEL = "gpt-4o"

def create_gadget_agent(
    mcp_server: MCPServerStreamableHttp,
) -> Agent:
    return Agent(
        model=AGENT_MODEL,
        name="Sage (Front of House)",
        instructions=get_sage_agent_instructions(),
        tools=[],
        mcp_servers=[mcp_server],
        model_settings=ModelSettings(
            temperature=0.7,
            # Slightly higher temperature for natural conversational style
        ),
    )
```

### 4. Wire into chatkit_server.py

In the `_get_agent_instructions` method (around line 2738), add:

```python
elif agent_type == "gadget":
    from app.agents.sage_instructions import get_sage_agent_instructions
    return get_sage_agent_instructions()
```

In the agent creation logic, add:

```python
elif agent_type == "gadget":
    from app.agents.sage_instructions import create_gadget_agent
    agent = create_gadget_agent(mcp_server=saas_frame_mcp)
```

### 5. Add API route for gadget conversations

server-gadget calls server-chatkit with `agentType: "gadget"`. Ensure the chatkit API route accepts this agent type and routes to the Sage agent.

## Design Decisions

- **30-word response cap:** Sage responses are spoken aloud through a 3W speaker. Long responses are unusable.
- **Read + Create only:** Sage cannot modify or delete back-of-house data. This is a security boundary — a compromised gadget on the restaurant floor shouldn't be able to corrupt inventory or financial data.
- **No business_context:** The pre-computed analytics aggregations are back-of-house only. Sage doesn't need supplier spend analysis or daypart profitability.
- **UK English:** Crunch's primary market is UK hospitality. Sage uses British spelling and phrasing.
- **No Square MCP:** For v1, Sage only connects to server-ops-mcp. Square data access (payments, loyalty) is a Phase 2 addition.
