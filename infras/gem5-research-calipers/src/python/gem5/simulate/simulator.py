# Copyright (c) 2021 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import m5
import m5.ticks
from m5.stats import addStatVisitor
from m5.stats.gem5stats import get_simstat
from m5.objects import Root
from m5.util import warn

import os
from pathlib import Path
from typing import Optional, List, Tuple, Dict, Generator, Union

from .exit_event_generators import (
    default_exit_generator,
    default_switch_generator,
    default_workbegin_generator,
    default_workend_generator,
)
from .exit_event import ExitEvent
from ..components.boards.abstract_board import AbstractBoard
from ..components.processors.cpu_types import CPUTypes


class Simulator:
    """
    This Simulator class is used to manage the execution of a gem5 simulation.

    **Warning:** The simulate package is still in a beta state. The gem5
    project does not guarantee the APIs within this package will remain
    consistent in future across upcoming releases.

    Example
    -------
    Examples using the Simulator class can be found under
    `configs/example/gem5_library`.

    The most basic run would be as follows:

    ```
    simulator = Simulator(board=board)
    simulator.run()
    ```

    This will run a simulation and execute default behavior for exit events.
    """

    def __init__(
        self,
        board: AbstractBoard,
        full_system: Optional[bool] = None,
        on_exit_event: Optional[
            Dict[Union[str, ExitEvent], Generator[Optional[bool], None, None]]
        ] = None,
        expected_execution_order: Optional[List[ExitEvent]] = None,
        checkpoint_path: Optional[Path] = None,
    ) -> None:
        """
        :param board: The board to be simulated.
        :param full_system: Whether to run as a full-system simulation or not.
        This is optional and used to override default behavior. If not set,
        whether or not to run in FS mode will be determined via the board's
        `is_fullsystem()` function.
        :param on_exit_event: An optional map to specify the generator to
        execute on each exit event. The generator may yield a boolean which,
        if True, will have the Simulator exit the run loop.
        :param expected_execution_order: May be specified to check the exit
        events come in a specified order. If the order specified is not
        encountered (e.g., 'Workbegin', 'Workend', then 'Exit'), an Exception
        is thrown. If this parameter is not specified, any ordering of exit
        events is valid.
        :param checkpoint_path: An optional parameter specifying the directory
        of the checkpoint to instantiate from. When the path is None, no
        checkpoint will be loaded. By default, the path is None.

        `on_exit_event` usage notes
        ---------------------------

        The `on_exit_event` parameter specifies a Python generator for each
        exit event. `next(<generator>)` is run each time an exit event. The
        generator may yield a boolean. If this value of this boolean is True
        the Simulator run loop will exit, otherwise
        the Simulator run loop will continue execution. If the generator has
        finished (i.e. a `StopIteration` exception is thrown when
        `next(<generator>)` is executed), then the default behavior for that
        exit event is run.

        As an example, a user may specify their own exit event setup like so:

        ```
        def unique_exit_event():
            processor.switch()
            yield False
            m5.stats.dump()
            yield False
            yield True

        simulator = Simulator(
            board=board
            on_exit_event = {
                ExitEvent.Exit : unique_exit_event(),
            },
        )
        ```

        This will execute `processor.switch()` the first time an exit event is
        encountered, will dump gem5 statistics the second time an exit event is
        encountered, and will terminate the Simulator run loop the third time.

        Each exit event has a default behavior if none is specified by the
        user. These are as follows:

            * ExitEvent.EXIT:  default_exit_list
            * ExitEvent.CHECKPOINT: default_exit_list
            * ExitEvent.FAIL : default_exit_list
            * ExitEvent.SWITCHCPU: default_switch_list
            * ExitEvent.WORKBEGIN: default_workbegin_list
            * ExitEvent.WORKEND: default_workend_list
            * ExitEvent.USER_INTERRUPT: default_exit_generator
            * ExitEvent.MAX_TICK: default_exit_generator()

        These generators can be found in the `exit_event_generator.py` module.

        """

        warn(
            "The simulate package is still in a beta state. The gem5 "
            "project does not guarantee the APIs within this package will "
            "remain consistent across upcoming releases."
        )

        # We specify a dictionary here outlining the default behavior for each
        # exit event. Each exit event is mapped to a generator.
        self._default_on_exit_dict = {
            ExitEvent.EXIT: default_exit_generator(),
            # TODO: Something else should be done here for CHECKPOINT
            ExitEvent.CHECKPOINT: default_exit_generator(),
            ExitEvent.FAIL: default_exit_generator(),
            ExitEvent.SWITCHCPU: default_switch_generator(
                processor=board.get_processor()
            ),
            ExitEvent.WORKBEGIN: default_workbegin_generator(),
            ExitEvent.WORKEND: default_workend_generator(),
            ExitEvent.USER_INTERRUPT: default_exit_generator(),
            ExitEvent.MAX_TICK: default_exit_generator(),
        }

        if on_exit_event:
            self._on_exit_event = on_exit_event
        else:
            self._on_exit_event = self._default_on_exit_dict

        self._instantiated = False
        self._board = board
        self._full_system = full_system
        self._expected_execution_order = expected_execution_order
        self._tick_stopwatch = []

        self._last_exit_event = None
        self._exit_event_count = 0

        self._checkpoint_path = checkpoint_path

    def get_stats(self) -> Dict:
        """
        Obtain the current simulation statistics as a Dictionary, conforming
        to a JSON-style schema.

        **Warning:** Will throw an Exception if called before `run()`. The
        board must be initialized before obtaining statistics
        """

        if not self._instantiated:
            raise Exception(
                "Cannot obtain simulation statistics prior to inialization."
            )

        return get_simstat(self._root).to_json()

    def add_text_stats_output(self, path: str) -> None:
        """
        This function is used to set an output location for text stats. If
        specified, when stats are dumped they will be output to this location
        as a text file file, in addition to any other stats' output locations
        specified.

        :param path: That path in which the file should be output to.
        """
        if not os.is_path_exists_or_creatable(path):
            raise Exception(
                f"Path '{path}' is is not a valid text stats output location."
            )
        addStatVisitor(path)

    def add_json_stats_output(self, path: str) -> None:
        """
        This function is used to set an output location for JSON. If specified,
        when stats are dumped they will be output to this location as a JSON
        file, in addition to any other stats' output locations specified.

        :param path: That path in which the JSON should be output to.
        """
        if not os.is_path_exists_or_creatable(path):
            raise Exception(
                f"Path '{path}' is is not a valid JSON output location."
            )
        addStatVisitor(f"json://{path}")

    def get_last_exit_event_cause(self) -> str:
        """
        Returns the last exit event cause.
        """
        return self._last_exit_event.getCause()

    def get_current_tick(self) -> int:
        """
        Returns the current tick.
        """
        return m5.curTick()

    def get_tick_stopwatch(self) -> List[Tuple[ExitEvent, int]]:
        """
        Returns a list of tuples, which each tuple specifying an exit event
        and the ticks at that event.
        """
        return self._tick_stopwatch

    def get_roi_ticks(self) -> List[int]:
        """
        Returns a list of the tick counts for every ROI encountered (specified
        as a region of code between a Workbegin and Workend exit event).
        """
        start = 0
        to_return = []
        for (exit_event, tick) in self._tick_stopwatch:
            if exit_event == ExitEvent.WORKBEGIN:
                start = tick
            elif exit_event == ExitEvent.WORKEND:
                to_return.append(tick - start)

        return to_return

    def _instantiate(self) -> None:
        """
        This method will instantiate the board and carry out necessary
        boilerplate code before the instantiation such as setting up root and
        setting the sim_quantum (if running in KVM mode).
        """

        if not self._instantiated:
            root = Root(
                full_system=self._full_system
                if self._full_system is not None
                else self._board.is_fullsystem(),
                board=self._board,
            )

            # We take a copy of the Root in case it's required elsewhere
            # (for example, in `get_stats()`).
            self._root = root

            if CPUTypes.KVM in [
                core.get_type()
                for core in self._board.get_processor().get_cores()
            ]:
                m5.ticks.fixGlobalFrequency()
                root.sim_quantum = m5.ticks.fromSeconds(0.001)

            # m5.instantiate() takes a parameter specifying the path to the
            # checkpoint directory. If the parameter is None, no checkpoint
            # will be restored.
            m5.instantiate(self._checkpoint_path)
            self._instantiated = True

    def run(self, max_ticks: int = m5.MaxTick) -> None:
        """
        This function will start or continue the simulator run and handle exit
        events accordingly.

        :param max_ticks: The maximum number of ticks to execute per simulation
        run. If this max_ticks value is met, a MAX_TICK exit event is
        received, if another simulation exit event is met the tick count is
        reset. This is the **maximum number of ticks per simululation run**.
        """

        # We instantiate the board if it has not already been instantiated.
        self._instantiate()

        # This while loop will continue until an a generator yields True.
        while True:

            self._last_exit_event = m5.simulate(max_ticks)

            # Translate the exit event cause to the exit event enum.
            exit_enum = ExitEvent.translate_exit_status(
                self.get_last_exit_event_cause()
            )

            # Check to see the run is corresponding to the expected execution
            # order (assuming this check is demanded by the user).
            if self._expected_execution_order:
                expected_enum = self._expected_execution_order[
                    self._exit_event_count
                ]
                if exit_enum.value != expected_enum.value:
                    raise Exception(
                        f"Expected a '{expected_enum.value}' exit event but a "
                        f"'{exit_enum.value}' exit event was encountered."
                    )

            # Record the current tick and exit event enum.
            self._tick_stopwatch.append((exit_enum, self.get_current_tick()))

            try:
                # If the user has specified their own generator for this exit
                # event, use it.
                exit_on_completion = next(self._on_exit_event[exit_enum])
            except StopIteration:
                # If the user's generator has ended, throw a warning and use
                # the default generator for this exit event.
                warn(
                    "User-specified generator for the exit event "
                    f"'{exit_enum.value}' has ended. Using the default "
                    "generator."
                )
                exit_on_completion = next(
                    self._default_on_exit_dict[exit_enum]
                )
            except KeyError:
                # If the user has not specified their own generator for this
                # exit event, use the default.
                exit_on_completion = next(
                    self._default_on_exit_dict[exit_enum]
                )

            self._exit_event_count += 1

            # If the generator returned True we will return from the Simulator
            # run loop.
            if exit_on_completion:
                return

    def save_checkpoint(self, checkpoint_dir: Path) -> None:
        """
        This function will save the checkpoint to the specified directory.

        :param checkpoint_dir: The path to the directory where the checkpoint
        will be saved.
        """
        m5.checkpoint(str(checkpoint_dir))

