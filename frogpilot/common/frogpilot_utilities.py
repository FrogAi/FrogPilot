#!/usr/bin/env python3
import capnp

from openpilot.frogpilot.common import frogpilot_paths


def load_log_schema(git_commit):
  schema_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / git_commit / "log.capnp"

  return capnp.SchemaParser().load(str(schema_path), imports=[str(schema_path.parent)])
