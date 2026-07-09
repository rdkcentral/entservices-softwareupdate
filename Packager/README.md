# Packager

## Overview

Packager is a Thunder plugin responsible for package-management integration in this repository.
It is built as a shared plugin module and uses libopkg integration for package operations.

## Plugin Identity and Runtime Model

- Callsign: Packager (not explicitly declared in Packager.conf.in or Packager.config templates)
- Hosting model: Thunder plugin shared library
- Runtime mode and autostart behavior are controlled by plugin config and CMake options

## Build Notes

From plugin build configuration:

- Plugin target is built from:
  - Module.cpp
  - Packager.cpp
  - PackagerImplementation.cpp
- Depends on:
  - Thunder plugins framework
  - LibOPKG
  - Optional libprovision support when available
- Installs plugin library into Thunder plugin destination path
- Installs generated opkg.conf into plugin share path

## Configuration Inputs

- Packager.config
- Packager.conf.in
- opkg.conf.in

## Source Layout

- Packager.h and Packager.cpp: plugin interface and entry points
- PackagerImplementation.h and PackagerImplementation.cpp: implementation details
- Module.h and Module.cpp: Thunder module wiring
- cmake/FindLibOPKG.cmake: dependency discovery helper

## Repository Context

This README is a plugin entry point for repository navigation.
For architecture and API details, use the plugin source and tests in this repository as the current source of truth.
