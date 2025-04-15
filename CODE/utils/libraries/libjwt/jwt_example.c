#include <stdio.h>
#include <stdlib.h>
#include <jwt.h>

int main(void) {
    jwt_t *jwt = NULL;
    const char *encoded;
    int ret;

    // Create a new JWT token
    ret = jwt_new(&jwt);
    if (ret != 0) {
        fprintf(stderr, "Error creating JWT token\n");
        return 1;
    }

    // Set the signing algorithm (HS256) and secret key ("secret")
    ret = jwt_set_alg(jwt, JWT_ALG_HS256, (unsigned char *)"secret", 6);
    if (ret != 0) {
        fprintf(stderr, "Error setting algorithm\n");
        jwt_free(jwt);
        return 1;
    }

    // Add some claims to the token
    jwt_add_grant(jwt, "iss", "your-issuer");
    jwt_add_grant(jwt, "sub", "your-subject");
    jwt_add_grant_int(jwt, "exp", 9999999999);

    // Encode the token into a string
    encoded = jwt_encode_str(jwt);
    if (!encoded) {
        fprintf(stderr, "Error encoding JWT\n");
        jwt_free(jwt);
        return 1;
    }

    printf("Encoded JWT: %s\n", encoded);

    // Free the JWT structure
    jwt_free(jwt);
    return 0;
}
