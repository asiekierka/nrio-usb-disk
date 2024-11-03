# nrio-usb-disk

Open source USB mass storage tool for Slot-1 NDS cartridges in conjunction with the NRIO/DS Linker Writer Slot-2 cartridge.

Improvements over the official tool, uDisk:

* Works with any DLDI-compatible Slot-1 cartridge (including >16K DLDI)
* Works with early "D12" boards (the final release of uDisk has unreliable behaviour on them)
* Slightly improved (+~10-20%) transfer rates
* Based on [an USB stack that's not two decades out of date](https://docs.tinyusb.org/en/latest/)

More information about the NRIO/DS Linker Writer is available [here](https://wiki.asie.pl/doku.php?id=notes:flashcart:ds_linker_writer).

## License

MIT
