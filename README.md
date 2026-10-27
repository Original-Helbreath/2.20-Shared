# 2.20-Shared

Code shared by the Helbreath 2.20 rebuild's Login server, World server and client, so each speaks the network
protocol from one copy instead of three.

- `protocol/` - framing (`frame_header`, `frame_codec`), whole-struct reading and writing (`span_reader`,
  `span_writer`), shared field types (`fixed_string`, `common_types`), the `message_id` enum, the packet
  structs (`login_messages` client <-> Login, `link_messages` Login <-> World), their receipt checks
  (`field_checks`) and typed send/receive (`message_codec`).

Standard library only, no dependency on any one program. Each consumer pulls this repo in as a git submodule
and pins the commit it builds against.
