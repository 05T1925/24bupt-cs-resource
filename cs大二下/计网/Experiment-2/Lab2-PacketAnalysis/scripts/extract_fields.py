from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CAPTURES_DIR = PROJECT_ROOT / "captures"
EXPORTS_DIR = PROJECT_ROOT / "exports"
DEFAULT_TSHARK = Path(r"C:\Program Files\Wireshark\tshark.exe")


COMMON_ARGS = [
    "-T",
    "fields",
    "-E",
    "header=y",
    "-E",
    "separator=,",
    "-E",
    "quote=d",
    "-E",
    "occurrence=a",
]


EXPORTS = {
    "icmp": {
        "capture": "icmp.pcapng",
        "display_filter": "icmp",
        "output": "icmp_fields.csv",
        "fields": [
            "frame.number",
            "frame.time_relative",
            "ip.src",
            "ip.dst",
            "ip.ttl",
            "ip.proto",
            "ip.len",
            "icmp.type",
            "icmp.code",
            "icmp.checksum",
            "icmp.ident",
            "icmp.ident_le",
            "icmp.seq",
            "icmp.seq_le",
            "data.len",
        ],
    },
    "ip_fragment": {
        "capture": "ip_fragment.pcapng",
        "display_filter": "ip.flags.mf == 1 || ip.frag_offset > 0",
        "output": "ip_fragment_fields.csv",
        "fields": [
            "frame.number",
            "frame.time_relative",
            "ip.src",
            "ip.dst",
            "ip.hdr_len",
            "ip.len",
            "ip.id",
            "ip.flags.df",
            "ip.flags.mf",
            "ip.frag_offset",
            "ip.proto",
            "icmp.type",
            "icmp.code",
        ],
    },
    "tcp": {
        "capture": "tcp.pcapng",
        "display_filter": "tcp.port == 80",
        "output": "tcp_fields.csv",
        "fields": [
            "frame.number",
            "frame.time_relative",
            "ip.src",
            "tcp.srcport",
            "ip.dst",
            "tcp.dstport",
            "tcp.stream",
            "tcp.flags",
            "tcp.flags.syn",
            "tcp.flags.ack",
            "tcp.flags.fin",
            "tcp.flags.reset",
            "tcp.seq",
            "tcp.ack",
            "tcp.len",
        ],
    },
}


DHCP_FIELDS = [
    "frame.number",
    "frame.time_relative",
    "eth.src",
    "eth.dst",
    "ip.src",
    "ip.dst",
    "udp.srcport",
    "udp.dstport",
    "{prefix}.id",
    "{prefix}.hw.mac_addr",
    "{prefix}.ip.client",
    "{prefix}.ip.your",
    "{prefix}.option.dhcp",
    "{prefix}.option.requested_ip_address",
    "{prefix}.option.dhcp_server_id",
    "{prefix}.option.subnet_mask",
    "{prefix}.option.router",
    "{prefix}.option.domain_name_server",
    "{prefix}.option.ip_address_lease_time",
]


def run_checked(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, capture_output=True, check=False)


def resolve_tshark(value: str | None) -> Path:
    candidate = Path(value) if value else DEFAULT_TSHARK
    if not candidate.is_file():
        raise FileNotFoundError(f"TShark not found: {candidate}")
    return candidate


def available_fields(tshark: Path) -> set[str]:
    result = run_checked([str(tshark), "-G", "fields"])
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode("utf-8", errors="replace"))

    fields: set[str] = set()
    for line in result.stdout.decode("utf-8", errors="replace").splitlines():
        parts = line.split("\t")
        if len(parts) >= 3 and parts[0] == "F":
            fields.add(parts[2])
    return fields


def dhcp_export(known_fields: set[str]) -> dict[str, object]:
    if "dhcp.option.dhcp" in known_fields:
        prefix = "dhcp"
    elif "bootp.option.dhcp" in known_fields:
        prefix = "bootp"
    else:
        raise RuntimeError("Neither dhcp.option.dhcp nor bootp.option.dhcp exists.")

    fields = [field.format(prefix=prefix) for field in DHCP_FIELDS]
    return {
        "capture": "dhcp.pcapng",
        "display_filter": "dhcp || bootp",
        "output": "dhcp_fields.csv",
        "fields": fields,
    }


def verify_fields(known_fields: set[str], fields: list[str]) -> None:
    missing = [field for field in fields if field not in known_fields]
    if missing:
        raise RuntimeError("Unsupported TShark fields: " + ", ".join(missing))


def export_csv(
    tshark: Path,
    definition: dict[str, object],
    raw_tcp_sequence_numbers: bool = False,
) -> tuple[Path, int]:
    capture = CAPTURES_DIR / str(definition["capture"])
    output = EXPORTS_DIR / str(definition["output"])
    fields = [str(field) for field in definition["fields"]]

    command = [str(tshark), "-r", str(capture)]
    if raw_tcp_sequence_numbers:
        command.extend(["-o", "tcp.relative_sequence_numbers:FALSE"])
    command.extend(["-Y", str(definition["display_filter"]), *COMMON_ARGS])
    for field in fields:
        command.extend(["-e", field])

    result = run_checked(command)
    if result.returncode != 0:
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"TShark export failed for {capture.name}: {error}")

    text = result.stdout.decode("utf-8-sig", errors="replace")
    rows = text.splitlines()
    data_rows = max(0, len(rows) - 1)
    if data_rows == 0:
        raise RuntimeError(
            f"No matching packets in {capture.name}; refusing to create empty CSV."
        )

    output.write_text(text, encoding="utf-8", newline="")
    return output, data_rows


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export traceable protocol fields from Lab 2 pcapng files."
    )
    parser.add_argument("--tshark", help="Full path to tshark.exe")
    parser.add_argument(
        "--protocol",
        choices=["all", "dhcp", "icmp", "ip_fragment", "tcp"],
        default="all",
    )
    parser.add_argument(
        "--preflight",
        action="store_true",
        help="Check TShark and field compatibility without requiring captures.",
    )
    args = parser.parse_args()

    try:
        tshark = resolve_tshark(args.tshark)
        version = run_checked([str(tshark), "-v"])
        if version.returncode != 0:
            raise RuntimeError("TShark version check failed.")
        version_line = version.stdout.decode("utf-8", errors="replace").splitlines()[0]
        print(f"[PASS] {version_line}")

        known_fields = available_fields(tshark)
        definitions = {"dhcp": dhcp_export(known_fields), **EXPORTS}
        selected = (
            list(definitions)
            if args.protocol == "all"
            else [args.protocol]
        )
        failed = False

        for name in selected:
            definition = definitions[name]
            fields = [str(field) for field in definition["fields"]]
            verify_fields(known_fields, fields)
            capture = CAPTURES_DIR / str(definition["capture"])
            print(f"[PASS] {name}: {len(fields)} fields supported")

            if args.preflight:
                print(
                    f"[INFO] {name}: capture "
                    f"{'exists' if capture.is_file() else 'not yet present'}"
                )
                continue

            if not capture.is_file():
                print(f"[FAIL] Missing capture: {capture}")
                failed = True
                continue

            output, count = export_csv(tshark, definition)
            print(f"[PASS] {name}: {count} rows -> {output}")

            if name == "tcp":
                raw_definition = dict(definition)
                raw_definition["output"] = "tcp_fields_rawseq.csv"
                raw_output, raw_count = export_csv(
                    tshark, raw_definition, raw_tcp_sequence_numbers=True
                )
                print(f"[PASS] tcp raw sequence: {raw_count} rows -> {raw_output}")

        return 1 if failed else 0
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
