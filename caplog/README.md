# `caplog`

Various tools to dissect capture logs of the communication between the STM and ESP.

## `dissect`

Takes in either a CSV from the Saleae software or a raw `bin` file containing a dump of all bytes transferred, and converts them to a visually
interpretable format.

`caplog/dissect.py [logicexport,simlog] <path to in file>`
(you can also use any prefix of the first argument)
