# Distributed Storage Service
**CS60002 - Distributed Systems Term Project**

## Setting Up Git Hooks

To ensure that the required Git hooks are active, follow these steps:

1. **Clone the repository** and navigate to its root directory:
    ```sh
    cd your-repo
    ```

2. **Run the setup script**:
    ```sh
    ./setup-hooks.sh
    ```

    This step is necessary only once. After running the script, Git will automatically apply the pre-commit hook whenever you commit new files.

3. **If you face any issues**, ensure the script has execution permissions by running:
    ```sh
    chmod +x setup-hooks.sh
    ```
