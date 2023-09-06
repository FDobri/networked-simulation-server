"""Run the same loopback workload at each requested client count."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("server", type=Path)
parser.add_argument("client", type=Path)
parser.add_argument("--counts", type=int, nargs="+", default=[1, 8, 32, 64])
parser.add_argument("--seconds", type=int, default=5)
args = parser.parse_args()

for count in args.counts:
    with subprocess.Popen([args.server.resolve(), "0", str(args.seconds + 2)],
                          stdout=subprocess.PIPE, text=True) as server:
        try:
            ready = server.stdout.readline().strip()
            if not ready.startswith("port="):
                raise RuntimeError(f"server did not announce a port: {ready}")
            subprocess.run([args.client.resolve(), "127.0.0.1", ready[5:], str(count),
                            str(args.seconds)], check=True, timeout=args.seconds + 3)
            lines, _ = server.communicate(timeout=args.seconds + 3)
            print(lines.strip(), flush=True)
            if server.returncode:
                raise subprocess.CalledProcessError(server.returncode, server.args)
        finally:
            if server.poll() is None:
                server.kill()
