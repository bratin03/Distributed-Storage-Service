import jwt
import logging
from jwt import InvalidTokenError, ExpiredSignatureError, InvalidSignatureError
from typing import Optional

logger = logging.getLogger("AuthLogger")
logging.basicConfig(level=logging.DEBUG)

PUBLIC_KEY_PATH = "public.pem"
_redis_cache = {}  # internal cache for lazy loading

ISSUER = "auth-server"

def _load_public_key() -> str:
    logger.debug("Loading public key from file")
    with open(PUBLIC_KEY_PATH, 'r') as f:
        return f.read()

def _get_public_key() -> str:
    if "public_key" not in _redis_cache:
        _redis_cache["public_key"] = _load_public_key()
    return _redis_cache["public_key"]

def verify_jwt(token: str) -> Optional[str]:
    logger.debug("Inside verify_jwt")

    if not token:
        logger.error("JWT Format Error: Empty token")
        return None

    if token.count('.') != 2:
        logger.error("JWT Format Error: Incorrect token structure")
        return None

    logger.debug("JWT Format Correct")

    try:
        decoded = jwt.decode(
            token,
            _get_public_key(),
            algorithms=["RS256"],
            issuer=ISSUER
        )
        return decoded.get("userID")
    except (ExpiredSignatureError, InvalidSignatureError, InvalidTokenError) as e:
        logger.error(f"JWT Verification Failed: {str(e)}")
        return None
    except Exception as e:
        logger.error(f"JWT Verification Failed: {str(e)}")
        return None
