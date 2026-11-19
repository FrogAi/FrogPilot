import asyncio

from openpilot.frogpilot.system.the_pond import tools, utilities


async def get_shell_pane(mocker, close_before_capture):
  the_pond = mocker.MagicMock(shell_used=None)
  run_process = utilities.run_process

  async def run_process_with_close(cmd):
    if close_before_capture and "capture-pane" in cmd:
      await tools.close_shell(the_pond)
    return await run_process(cmd)

  mocker.patch.object(utilities, "run_process", run_process_with_close)
  try:
    return await tools.get_tmux_pane(mocker.Mock(app={utilities.THE_POND: the_pond}, query={"session": "shell"}))
  finally:
    await tools.close_shell(the_pond)


def test_shell_closed_during_capture_has_no_output(mocker):
  assert asyncio.run(get_shell_pane(mocker, True)).status == 204


def test_open_shell_returns_its_output(mocker):
  assert asyncio.run(get_shell_pane(mocker, False)).status == 200
