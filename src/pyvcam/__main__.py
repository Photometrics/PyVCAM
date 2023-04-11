#!/usr/bin/env python3
"""
__main__.py

Implement ARTIQ Network Device Support Package (NDSP) to support Teledyne PrimeBSI camera integration into ARTIQ experiment.

Kevin Chen
2023-02-24
University of Waterloo
QuantumIon
"""

from sipyco.pc_rpc import simple_server_loop
from sipyco import common_args
import argparse
import logging
from pyvcam.driver import PyVCAM


logger = logging.getLogger(__name__)

def get_argparser():
    parser = argparse.ArgumentParser(description="""PyVCAM controller. Use this controller to drive the Teledyne PrimeBSI camera.
                                     See documentation at https://github.com/Photometrics/PyVCAM""")
    common_args.simple_network_args(parser, 3249)
    common_args.verbosity_args(parser)
    return parser

def main():
    args = get_argparser().parse_args()
    common_args.init_logger_from_args(args)
    camera = PyVCAM()

    try:
        camera.open()
        logger.info('PyVCAM open.')
        simple_server_loop({"pyvcam": camera}, common_args.bind_address_from_args(args), args.port)
    finally:
        camera.close()
        logger.info('PyVCAM closed.')
        del camera

if __name__ == "__main__":
    main()
