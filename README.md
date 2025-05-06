# FlowGraph Narrative Plugin

***This plugin is still in development and is not ready for production use.***

## Overview

This plugin extends the core **Flow Graph** system (https://github.com/MothCocoon/FlowGraph) to provide a robust framework for creating complex narrative experiences within Unreal Engine 5.5. It acts as a powerful plugin for narrative systems, enabling features like branching dialogue, Quick Time Events (QTEs), object inspection mechanics, memory tracking of player interactions, and managing story facts.

Leveraging the flexibility of Flow Graph, this plugin integrates seamlessly with **InkCPP** for powerful ink script support, allowing designers to write intricate narrative logic.

## Key Features

*   **Branching Dialogue:** Design complex conversations with multiple paths and outcomes using Flow Graph nodes and Ink integration.
*   **Interaction System:**
    *   **QTEs (Quick Time Events):** Implement timed input challenges directly within the narrative flow.
    *   **Object Inspection:** Allow players to examine objects in the world, potentially triggering dialogue or updating story facts.
*   **Memory Tracking:** Keep track of player choices and interactions, influencing future dialogue options and events.
*   **Story Facts:** Manage the state of the narrative world, tracking key events and information that can gate progress or change outcomes.
*   **InkCPP Integration:** Utilize the full power of the Ink scripting language for narrative content, parsed and executed via InkCPP.
*   **Enhanced Input:** Makes best use of Unreal Engine's Enhanced Input system for flexible and context-aware player controls during narrative sequences.
*   **UE 5.5 Sequencer Integration:**
    *   **Dynamic Binding:** Easily bind narrative events and characters to Sequencer actors.
    *   **Conditional Track Control:** Activate or deactivate Sequencer tracks and sections based on narrative state (Story Facts, Memory).
    *   **Time Warping:** Control the playback speed of Sequences, Subsequences, or Skeletal Animations dynamically using authorable Curves driven by narrative logic.

## Dependencies

*   Flow Graph Plugin (Must be added to the project separately)
*   InkCPP (Must be added to the project separately)
*   Unreal Engine 5.5+
