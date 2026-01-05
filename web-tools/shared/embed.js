export const EMBEDDED_QUERY_PARAM = 'embedded';
export const EMBEDDED_VALUE = '1';

export const isEmbeddedRequest = (search = window.location.search) => {
    return new URLSearchParams(search).get(EMBEDDED_QUERY_PARAM) === EMBEDDED_VALUE;
};

export const applyEmbeddedState = (root, options = {}) => {
    const embedded = options.embedded ?? isEmbeddedRequest();

    if (root) {
        if (embedded) {
            root.dataset.embedded = EMBEDDED_VALUE;
        } else {
            delete root.dataset.embedded;
        }
    }

    return embedded;
};