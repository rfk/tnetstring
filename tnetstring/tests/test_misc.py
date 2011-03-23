
import os
import os.path
import difflib
import unittest
import doctest

import tnetstring


class Test_Misc(unittest.TestCase):

    def test_readme_matches_docstring(self):
        """Ensure that the README is in sync with the docstring.

        This test should always pass; if the README is out of sync it just
        updates it with the contents of tnetstring.__doc__.
        """
        dirname = os.path.dirname
        readme = os.path.join(dirname(dirname(dirname(__file__))),"README.rst")
        if not os.path.isfile(readme):
            f = open(readme,"wb")
            f.write(tnetstring.__doc__.encode())
            f.close()
        else:
            f = open(readme,"rb")
            if f.read() != tnetstring.__doc__:
                f.close()
                f = open(readme,"wb")
                f.write(tnetstring.__doc__.encode())
                f.close()

