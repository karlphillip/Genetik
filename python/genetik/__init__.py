"""Python interface to Genetik's existing native C++ regression engine."""

from .regressor import GenetikRegressor

__version__ = "0.5.0"
__all__ = ["GenetikRegressor", "__version__"]
