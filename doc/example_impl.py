from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional, Tuple, Callable

# --- Core Data Structures ---

class GameState(ABC):
    """
    Abstract base class for representing the current state of the game.
    This includes player inventory, character statuses, world flags, etc.
    """

    @abstractmethod
    def get_variable(self, variable_name: str) -> Any:
        """Gets the value of a game state variable."""
        pass

    @abstractmethod
    def set_variable(self, variable_name: str, value: Any) -> None:
        """Sets the value of a game state variable."""
        pass

    @abstractmethod
    def check_condition(self, condition: Dict[str, Any]) -> bool:
        """
        Checks if a complex condition (e.g., multiple variable checks) is met.
        Example condition: {"player_health_gt": 50, "has_item": "key_to_city"}
        """
        pass

    @abstractmethod
    def apply_effect(self, effect: Dict[str, Any]) -> None:
        """
        Applies an effect to the game state.
        Example effect: {"player_health_add": -10, "set_flag": "door_unlocked"}
        """
        pass

    @abstractmethod
    def get_context_for_llm(self) -> Dict[str, Any]:
        """Returns a dictionary representing the current game state relevant for LLM prompting."""
        pass

class LLMInterface(ABC):
    """
    Abstract base class for interacting with a Large Language Model.
    """

    @abstractmethod
    async def generate_text(
        self,
        prompt: str,
        context: Dict[str, Any], # GameState context + storylet-specific context
        constraints: List['NarrativeConstraint'], # LLM-specific generation constraints
        max_tokens: Optional[int] = None,
        temperature: Optional[float] = None,
        # Other LLM parameters as needed
    ) -> str:
        """Generates text based on a prompt, context, and constraints."""
        pass

    @abstractmethod
    async def generate_storylet_weights(
        self,
        current_game_state: GameState,
        candidate_storylets: List['SmartStorylet'],
        narrative_context: Dict[str, Any], # Broader narrative situation
        active_constraints: List['NarrativeConstraint']
    ) -> Dict[str, float]: # Mapping storylet_id to weight
        """
        (Optional advanced feature)
        Uses an LLM to evaluate candidate storylets and output weights for selection.
        """
        pass

# --- Narrative Components ---

class SmartStorylet(ABC):
    """
    Abstract base class for a self-contained narrative unit.
    """
    def __init__(self, storylet_id: str, author_tags: Optional[List[str]] = None):
        self.storylet_id = storylet_id
        self.author_tags = author_tags if author_tags else [] # For organization, theme, etc.

    @abstractmethod
    def get_id(self) -> str:
        """Returns the unique identifier of the storylet."""
        return self.storylet_id

    @abstractmethod
    def get_preconditions(self) -> List[Dict[str, Any]]:
        """
        Returns a list of conditions that must be true for this storylet to be eligible.
        Each condition is a dictionary, e.g., {"variable": "quest_A_status", "value": "active"}
        """
        pass

    @abstractmethod
    def check_preconditions(self, game_state: GameState) -> bool:
        """Checks if all preconditions are met by the current game state."""
        for condition in self.get_preconditions():
            if not game_state.check_condition(condition):
                return False
        return True

    @abstractmethod
    def get_effects(self) -> List[Dict[str, Any]]:
        """
        Returns a list of effects this storylet will have on the game state upon completion.
        Each effect is a dictionary, e.g., {"variable": "player_xp", "change": 100}
        """
        pass

    @abstractmethod
    def apply_effects(self, game_state: GameState) -> None:
        """Applies the storylet's effects to the game state."""
        for effect in self.get_effects():
            game_state.apply_effect(effect)

    @abstractmethod
    async def generate_content(
        self,
        game_state: GameState,
        llm_interface: Optional[LLMInterface] = None,
        active_constraints: Optional[List['NarrativeConstraint']] = None
    ) -> str:
        """
        Generates or retrieves the content of this storylet.
        This might involve authored text, LLM generation, or a combination.
        """
        pass

    @abstractmethod
    def get_llm_directives(self) -> Optional[Dict[str, Any]]:
        """
        Returns specific directives for LLM generation if this storylet uses an LLM.
        e.g., {"prompt_template": "Character A says to B: {dialogue_topic}", "tone": "suspicious"}
        """
        pass

    @abstractmethod
    def get_fallback_content(self) -> str:
        """Returns pre-authored fallback content if LLM generation fails or is not used."""
        pass

    @abstractmethod
    def get_metadata(self) -> Dict[str, Any]:
        """Returns metadata about the storylet (e.g., author, creation date, version, character involvement)."""
        pass


class NarrativeConstraint(ABC):
    """
    Abstract base class for a narrative constraint.
    Constraints can influence storylet selection or LLM generation.
    """
    def __init__(self, constraint_id: str, description: str, scope: str = "global"):
        self.constraint_id = constraint_id
        self.description = description
        self.scope = scope # e.g., "global", "local_to_flow_node_X", "character_Y"

    @abstractmethod
    def get_id(self) -> str:
        return self.constraint_id

    @abstractmethod
    def evaluate(self, game_state: GameState, candidate_storylet: Optional[SmartStorylet] = None) -> float:
        """
        Evaluates how well the current state or a candidate storylet satisfies this constraint.
        Returns a score (e.g., 0.0 to 1.0, or a penalty/bonus).
        If candidate_storylet is None, evaluates the constraint against the general game state.
        """
        pass

    @abstractmethod
    def get_llm_guidance(self) -> Optional[str]:
        """
        Returns a string that can be appended to an LLM prompt to enforce this constraint
        during text generation. e.g., "Ensure the dialogue maintains a formal tone."
        """
        pass


class NarrativeFlowGraphNode(ABC):
    """
    Abstract base class for a node in the Narrative FlowGraph.
    Nodes can represent individual storylets, clusters, or generative zones.
    """
    def __init__(self, node_id: str, node_type: str): # e.g., "storylet_cluster", "critical_path_point"
        self.node_id = node_id
        self.node_type = node_type

    @abstractmethod
    def get_id(self) -> str:
        return self.node_id

    @abstractmethod
    def get_connected_nodes(self) -> List[Tuple[str, Dict[str, Any]]]: # List of (node_id, transition_conditions)
        """Returns IDs of nodes connected from this node and conditions for transitioning."""
        pass

    @abstractmethod
    def get_associated_storylets(self) -> List[str]: # List of storylet_ids
        """Returns IDs of storylets directly associated or contained within this node."""
        pass

    @abstractmethod
    def on_enter(self, game_state: GameState, narrative_engine: 'NarrativeEngine') -> None:
        """Logic to execute when this node becomes active in the narrative flow."""
        pass

    @abstractmethod
    def on_exit(self, game_state: GameState, narrative_engine: 'NarrativeEngine') -> None:
        """Logic to execute when the narrative flow leaves this node."""
        pass


class Bark(ABC):
    """
    Abstract base class for a short, reactive piece of dialogue or text.
    """
    def __init__(self, bark_id: str, trigger_event: str):
        self.bark_id = bark_id
        self.trigger_event = trigger_event # e.g., "player_low_health", "item_pickup_X"

    @abstractmethod
    def get_id(self) -> str:
        return self.bark_id

    @abstractmethod
    def check_trigger(self, event_type: str, event_data: Dict[str, Any], game_state: GameState) -> bool:
        """Checks if this bark should be triggered by the given event and game state."""
        pass

    @abstractmethod
    async def generate_content(
        self,
        game_state: GameState,
        event_data: Dict[str, Any],
        llm_interface: Optional[LLMInterface] = None,
        active_constraints: Optional[List['NarrativeConstraint']] = None
    ) -> str:
        """Generates the bark content, potentially using an LLM."""
        pass

    @abstractmethod
    def get_llm_prompt_template(self) -> Optional[str]:
        """Returns a specific prompt template for LLM generation if this bark uses an LLM."""
        pass

# --- Orchestration and Selection ---

class StoryletSelector(ABC):
    """
    Abstract base class for selecting the next storylet to activate.
    """

    @abstractmethod
    def find_eligible_storylets(
        self,
        all_storylets: List[SmartStorylet],
        current_game_state: GameState,
        current_flow_node: Optional[NarrativeFlowGraphNode] = None
    ) -> List[SmartStorylet]:
        """Filters storylets based on hard preconditions and current narrative flow context."""
        pass

    @abstractmethod
    async def select_next_storylet(
        self,
        eligible_storylets: List[SmartStorylet],
        current_game_state: GameState,
        active_constraints: List[NarrativeConstraint],
        narrative_history: List[str], # List of recently activated storylet IDs
        llm_interface: Optional[LLMInterface] = None # For LLM-assisted weighting
    ) -> Optional[SmartStorylet]:
        """
        Selects the best storylet from the eligible ones based on constraints,
        history (for Markovian influence), and potentially LLM-generated weights.
        """
        pass


class BarkSystem(ABC):
    """
    Abstract base class for managing and triggering barks.
    """
    def __init__(self, all_barks: List[Bark], llm_interface: Optional[LLMInterface] = None):
        self.all_barks = all_barks
        self.llm_interface = llm_interface

    @abstractmethod
    async def process_event(
        self,
        event_type: str,
        event_data: Dict[str, Any],
        game_state: GameState,
        active_constraints: List[NarrativeConstraint]
    ) -> List[str]: # Returns a list of generated bark contents
        """
        Processes a game event and returns any triggered bark content.
        """
        pass


class NarrativeEngine(ABC):
    """
    Abstract base class for the main narrative orchestrator.
    Manages game state, storylets, flow graph, constraints, and LLM interaction.
    """
    def __init__(
        self,
        game_state: GameState,
        llm_interface: Optional[LLMInterface],
        storylet_selector: StoryletSelector,
        all_storylets: List[SmartStorylet],
        narrative_flow_graph: Dict[str, NarrativeFlowGraphNode], # node_id -> Node object
        all_constraints: List[NarrativeConstraint],
        bark_system: Optional[BarkSystem] = None
    ):
        self.game_state = game_state
        self.llm_interface = llm_interface
        self.storylet_selector = storylet_selector
        self.all_storylets_map = {s.get_id(): s for s in all_storylets}
        self.narrative_flow_graph = narrative_flow_graph
        self.all_constraints_map = {c.get_id(): c for c in all_constraints}
        self.bark_system = bark_system
        self.narrative_history: List[str] = [] # Recently played storylet IDs
        self.current_flow_node_id: Optional[str] = None # ID of the active node in the flow graph

    @abstractmethod
    def initialize_narrative(self, starting_flow_node_id: str) -> None:
        """Initializes the narrative, setting the starting point in the flow graph."""
        pass

    @abstractmethod
    def get_active_constraints(self) -> List[NarrativeConstraint]:
        """Determines which constraints are currently active based on game state and flow graph location."""
        pass

    @abstractmethod
    async def advance_narrative(self) -> Optional[str]: # Returns content of the activated storylet
        """
        The main loop:
        1. Identify eligible storylets.
        2. Select the next storylet.
        3. Activate it (generate content, apply effects).
        4. Update narrative history and flow graph position.
        Returns the presented content or None if no storylet could be activated.
        """
        pass

    @abstractmethod
    async def trigger_barks(self, event_type: str, event_data: Dict[str, Any]) -> List[str]:
        """Allows external game systems to notify the narrative engine of events for bark processing."""
        if self.bark_system:
            active_constraints = self.get_active_constraints()
            return await self.bark_system.process_event(event_type, event_data, self.game_state, active_constraints)
        return []

    @abstractmethod
    def get_storylet_by_id(self, storylet_id: str) -> Optional[SmartStorylet]:
        return self.all_storylets_map.get(storylet_id)

    @abstractmethod
    def get_constraint_by_id(self, constraint_id: str) -> Optional[NarrativeConstraint]:
        return self.all_constraints_map.get(constraint_id)

    @abstractmethod
    def get_flow_node_by_id(self, node_id: str) -> Optional[NarrativeFlowGraphNode]:
        return self.narrative_flow_graph.get(node_id)


# --- Example: Authoring Tool Interface (Conceptual) ---

class AuthoringToolInterface(ABC):
    """
    Conceptual interface for tools that authors would use to create
    storylets, flow graphs, and constraints.
    """

    @abstractmethod
    def create_storylet(self, storylet_data: Dict[str, Any]) -> SmartStorylet:
        pass

    @abstractmethod
    def define_flow_graph_node(self, node_data: Dict[str, Any]) -> NarrativeFlowGraphNode:
        pass

    @abstractmethod
    def create_constraint(self, constraint_data: Dict[str, Any]) -> NarrativeConstraint:
        pass

    @abstractmethod
    def visualize_flow_graph(self, graph: Dict[str, NarrativeFlowGraphNode]) -> None:
        pass

    @abstractmethod
    def simulate_narrative_step(self, engine: NarrativeEngine) -> None:
        pass


if __name__ == '__main__':
    # This is where you would create concrete implementations of these abstract classes
    # and instantiate your narrative engine.
    # For now, it just demonstrates the structure.

    print("LEC-Flow Abstract Classes Defined.")
    print("Next steps would be to implement concrete classes for each of these components.")

    # Example of how one might think about using these (highly simplified):
    class MyGameState(GameState):
        def __init__(self): self._data = {}
        def get_variable(self, name): return self._data.get(name)
        def set_variable(self, name, value): self._data[name] = value; print(f"GameState: {name} = {value}")
        def check_condition(self, cond):
            # Simplified: assumes cond is like {"variable_name": "expected_value"}
            var_name, expected_val = list(cond.items())[0]
            return self._data.get(var_name) == expected_val
        def apply_effect(self, effect):
            # Simplified: assumes effect is like {"variable_name": "new_value"}
            var_name, new_val = list(effect.items())[0]
            self.set_variable(var_name, new_val)
        def get_context_for_llm(self): return self._data.copy()

    class MyStorylet(SmartStorylet):
        def __init__(self, storylet_id, preconditions, effects, content_template, llm_prompt=None, fallback="Default content."):
            super().__init__(storylet_id)
            self._preconditions = preconditions
            self._effects = effects
            self._content_template = content_template # e.g., "Character {char_name} says: {dialogue}"
            self._llm_prompt_key = llm_prompt # Key in game_state to use for LLM prompt, or direct prompt
            self._fallback = fallback

        def get_id(self): return self.storylet_id
        def get_preconditions(self): return self._preconditions
        def get_effects(self): return self._effects
        async def generate_content(self, game_state, llm_interface=None, active_constraints=None):
            if self._llm_prompt_key and llm_interface:
                prompt = game_state.get_variable(self._llm_prompt_key) or self._llm_prompt_key
                try:
                    # Simplified context for LLM
                    llm_context = game_state.get_context_for_llm()
                    llm_context["storylet_id"] = self.storylet_id
                    generated_dialogue = await llm_interface.generate_text(prompt, llm_context, active_constraints or [])
                    return self._content_template.format(dialogue=generated_dialogue, **llm_context)
                except Exception as e:
                    print(f"LLM generation failed for {self.storylet_id}: {e}")
                    return self._fallback.format(**game_state.get_context_for_llm())
            return self._content_template.format(**game_state.get_context_for_llm()) # Basic template filling
        def get_llm_directives(self): return {"prompt_key": self._llm_prompt_key} if self._llm_prompt_key else None
        def get_fallback_content(self): return self._fallback
        def get_metadata(self): return {"type": "dialogue"} # Example

    # Further concrete classes would be needed for LLMInterface, NarrativeConstraint, etc.
    # This is just to show the direction.