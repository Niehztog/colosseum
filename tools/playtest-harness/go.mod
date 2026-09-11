module q2playtest

go 1.22

// The protocol fixes the harness depends on are upstream as of v1.0.335 -- the
// fork branch was merged as packetflinger/libq2 PR #6 -- so libq2 is pinned by
// version here and no longer taken from a fork checkout.  `vendor/` still holds
// its code, so `go build` and `go run` need no network and no second checkout;
// `go mod vendor` refreshes it from the pinned release.
require github.com/packetflinger/libq2 v1.0.335

require google.golang.org/protobuf v1.34.2 // indirect
