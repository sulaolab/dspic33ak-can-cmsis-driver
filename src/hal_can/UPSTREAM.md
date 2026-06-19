# Upstream HAL Source

The files in this directory are a vendor copy of the dsPIC33AK CAN FD HAL.

Upstream repository:

- Repository: https://github.com/sulaolab/dspic33ak-can-hal
- Branch: pre-publish-qc (pre-merge; will be main at publish)
- Source directory: src/

Synchronized into this repository under:

- Destination directory: src/hal_can/

## Current Synchronized Revision

- Upstream commit: d923ec93a12b32d7bc1e4f92b890504c678d6828

This revision is synchronized from the upstream `main` branch.

## Update Policy

The HAL-only repository is the upstream source of truth.

Please apply HAL fixes and HAL feature changes to the upstream HAL repository first, then synchronize this vendor copy.

CMSIS-Driver wrapper changes belong in this repository.
