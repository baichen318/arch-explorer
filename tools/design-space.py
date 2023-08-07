import argparse
import numpy as np
import scipy.stats as stats
from sklearn.manifold import TSNE
from sklearn.neural_network import MLPRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LinearRegression
from sklearn.model_selection import train_test_split
from sklearn.gaussian_process import GaussianProcessRegressor
from sklearn.gaussian_process.kernels import DotProduct, WhiteKernel
from sklearn.metrics import mean_absolute_percentage_error, mean_absolute_error, mean_squared_error


def load_dataset(path):
    dataset_path = path
    feature_vec = []
    perf_vec = []
    power_vec = []
    area_vec = []
    with open(dataset_path) as f:
        lines = [line.rstrip() for line in f]
        for line in lines:
            point = np.array(line.split())
            feature_vec.append([float(e) for e in point[0:22]])
            area_vec.append(float(point[-1]))
            power_vec.append(float(point[-2]))
            perf_vec.append(float(point[-3]))
    
    features = np.asarray(feature_vec)
    perfs = np.array(perf_vec)
    powers = np.array(power_vec)
    areas = np.array(area_vec)

    return features, perfs, powers, areas


def space_split(X, y, split_factor = 5, test_set_id = 0):
    X_subsets = np.array_split(X, split_factor)
    y_subsets = np.array_split(y, split_factor)

    X_test = X_subsets[test_set_id]
    y_test = y_subsets[test_set_id]

    X_train_list = []
    y_train_list = []

    for i in range(split_factor):
        if i != test_set_id:
            X_train_list.append(X_subsets[i])
            y_train_list.append(y_subsets[i])

    X_train = np.concatenate(X_train_list, axis=0)
    y_train = np.concatenate(y_train_list, axis=0)

    return X_train, X_test, y_train, y_test


def save_dataset(features, perfs, powers, areas, path):
    with open(path, 'w') as f:
        for feature, perf, power, area in zip(features, perfs, powers, areas):
            point = [*(feature.tolist()), float(perf), float(power), float(area)]
            #print(' '.join(str(x) for x in point))
            f.write(' '.join(str(x) for x in point))
            f.write('\n')


def linear_regression(X, y, cross_validation_num = 5):
    kendall_tau = 0.0
    mape_final = 0.0
    spearman_tau = 0.0
    for i in range(cross_validation_num):
        X_train, X_test, y_train, y_test = space_split(X, y, cross_validation_num, test_set_id = i)
        model = LinearRegression()
        model.fit(X_train, y_train)

        y_pred = model.predict(X_test)

        tau, p_value = stats.kendalltau(y_test, y_pred)
        kendall_tau = kendall_tau + tau
        # print("kendall tau: {}".format(tau))
        # print("kendall p_value: {}".format(p_value))

        mape = mean_absolute_percentage_error(y_test, y_pred)
        mape_final = mape_final + mape
        # print("mean absolute percentage error: {}".format(mape))

        tau, p_value = stats.spearmanr(y_test, y_pred)
        spearman_tau = spearman_tau + tau
        # print("spearman correlation: {}".format(tau))
        # print("spearman p_value: {}".format(p_value))

    kendall_tau = kendall_tau / cross_validation_num
    mape_final = mape_final / cross_validation_num
    spearman_tau = spearman_tau / cross_validation_num

    print("kendall tau: {:>40.6f}".format(kendall_tau))
    print("mean absolute percentage error: {:>21.6f}".format(mape_final))
    print("spearman tau: {:>39.6f}".format(spearman_tau))


def neural_network(X, y, cross_validation_num = 5):
    kendall_tau = 0.0
    mape_final = 0.0
    spearman_tau = 0.0
    for i in range(cross_validation_num):
        X_train, X_test, y_train, y_test = space_split(X, y, cross_validation_num, test_set_id = i)

        # data preprocessing
        sc = StandardScaler()

        scaler = sc.fit(X_train)
        X_train_scaled = scaler.transform(X_train)
        X_test_scaled = scaler.transform(X_test)

        model = MLPRegressor(solver='lbfgs', alpha=0.001, max_iter = 300, activation = 'relu',
                             hidden_layer_sizes=(150,100,50), random_state=1)
        model.fit(X_train_scaled, y_train)

        y_pred = model.predict(X_test_scaled)

        tau, p_value = stats.kendalltau(y_test, y_pred)
        kendall_tau = kendall_tau + tau
        # print("kendall tau: {}".format(tau))
        # print("kendall p_value: {}".format(p_value))

        mape = mean_absolute_percentage_error(y_test, y_pred)
        mape_final = mape_final + mape
        # print("mean absolute percentage error: {}".format(mape))

        tau, p_value = stats.spearmanr(y_test, y_pred)
        spearman_tau = spearman_tau + tau
        # print("spearman correlation: {}".format(tau))
        # print("spearman p_value: {}".format(p_value))

    kendall_tau = kendall_tau / cross_validation_num
    mape_final = mape_final / cross_validation_num
    spearman_tau = spearman_tau / cross_validation_num

    print("kendall tau: {:>40.6f}".format(kendall_tau))
    print("mean absolute percentage error: {:>21.6f}".format(mape_final))
    print("spearman tau: {:>39.6f}".format(spearman_tau))

def gaussian_process(X, y, cross_validation_num = 5):
    kendall_tau = 0.0
    mape_final = 0.0
    spearman_tau = 0.0
    for i in range(cross_validation_num):
        X_train, X_test, y_train, y_test = space_split(X, y, cross_validation_num, test_set_id = i)

        kernel = DotProduct() + WhiteKernel()
        model = GaussianProcessRegressor(kernel=kernel, random_state=0)
        model.fit(X_train, y_train)

        y_pred = model.predict(X_test)

        tau, p_value = stats.kendalltau(y_test, y_pred)
        kendall_tau = kendall_tau + tau
        # print("kendall tau: {}".format(tau))
        # print("kendall p_value: {}".format(p_value))

        mape = mean_absolute_percentage_error(y_test, y_pred)
        mape_final = mape_final + mape
        # print("mean absolute percentage error: {}".format(mape))

        tau, p_value = stats.spearmanr(y_test, y_pred)
        spearman_tau = spearman_tau + tau
        # print("spearman correlation: {}".format(tau))
        # print("spearman p_value: {}".format(p_value))

    kendall_tau = kendall_tau / cross_validation_num
    mape_final = mape_final / cross_validation_num
    spearman_tau = spearman_tau / cross_validation_num

    print("kendall tau: {:>40.6f}".format(kendall_tau))
    print("mean absolute percentage error: {:>21.6f}".format(mape_final))
    print("spearman tau: {:>39.6f}".format(spearman_tau))


def create_pairs(X, y):
    X_pairs = []
    y_pairs = []

    for i in range(len(y)):
        for j in range(len(y)):
            if j > i:
                #X_pairs.append([X[i], X[j]])
                X_pairs.append(X[i] - X[j])
                if y[i] - y[j] > 1e-6:
                    y_pairs.append(1)
                else:
                    y_pairs.append(0)

    return X_pairs, y_pairs


def rank_boost(X, y, cross_validation_num = 5):
    kendall_tau = 0.0
    mape_final = 0.0
    spearman_tau = 0.0
    for i in range(cross_validation_num):
        print("cross validation step {}".format(i))
        X_train, X_test, y_train, y_test = space_split(X, y, cross_validation_num, test_set_id = i)

        X_train_pairs, y_train_pairs = create_pairs(X_train, y_train)
        X_train_pairs, y_train_pairs = np.array(X_train_pairs), np.array(y_train_pairs)

        model = BipartiteRankBoost()
        model.fit(X_train_pairs, y_train_pairs)

        X_test_pairs, y_test_pairs = create_pairs(X_test, y_test)
        X_test_pairs, y_test_pairs = np.array(X_test_pairs), np.array(y_test_pairs)
        y_pred = model.predict_proba(X_test_pairs)

        tau, p_value = stats.kendalltau(y_test_pairs, y_pred)
        kendall_tau = kendall_tau + tau
        # print("kendall tau: {}".format(tau))
        # print("kendall p_value: {}".format(p_value))

        # mape = mean_absolute_percentage_error(y_test_pairs, y_pred)
        mape = mean_absolute_percentage_error(y_pred, y_test_pairs)
        # print(y_test_pairs[0:5])
        # print(y_pred[0:5])
        # print("cur mape: {}".format(mape))
        mape_final = mape_final + mape
        # print("mean absolute percentage error: {}".format(mape))

        tau, p_value = stats.spearmanr(y_test_pairs, y_pred)
        spearman_tau = spearman_tau + tau
        # print("spearman correlation: {}".format(tau))
        # print("spearman p_value: {}".format(p_value))

    kendall_tau = kendall_tau / cross_validation_num
    mape_final = mape_final / cross_validation_num
    spearman_tau = spearman_tau / cross_validation_num

    print("kendall tau: {:>40.6f}".format(kendall_tau))
    print("mean absolute percentage error: {:>21.6f}".format(mape_final))
    print("spearman tau: {:>39.6f}".format(spearman_tau))


def run_linear_regression(features, perfs, powers, areas, cross_validation_num):
    print("---linear regression for performance---")
    linear_regression(features, perfs, cross_validation_num)
    print("---linear regression for power---")
    linear_regression(features, powers, cross_validation_num)
    print("---linear regression for area---")
    linear_regression(features, areas, cross_validation_num)


def run_neural_network(features, perfs, powers, areas, cross_validation_num):
    print("---neural network for performance---")
    neural_network(features, perfs, cross_validation_num)
    print("---neural network for power---")
    neural_network(features, powers, cross_validation_num)
    print("---neural network for area---")
    neural_network(features, areas, cross_validation_num)


def run_gaussian_process(features, perfs, powers, areas, cross_validation_num):
    # print("---gaussian process for performance---")
    # gaussian_process(features, perfs, cross_validation_num)
    # print("---gaussian process for power---")
    # gaussian_process(features, powers, cross_validation_num)
    print("---gaussian process for area---")
    gaussian_process(features, areas, cross_validation_num)


def run_rank_boost(features, perfs, powers, areas, cross_validation_num):
    #print("---rank boost for performance---")
    #rank_boost(features, perfs, cross_validation_num)
    print("---rank boost for power---")
    rank_boost(features, powers, cross_validation_num)
    print("---rank boost for area---")
    rank_boost(features, areas, cross_validation_num)
        

def run(args):
    cross_validation_num = 5
    features, perfs, powers, areas = load_dataset(args.dataset)

    # linear regression
    run_linear_regression(features, perfs, powers, areas, cross_validation_num)

    # neural network
    run_neural_network(features, perfs, powers, areas, cross_validation_num)

    # gaussian process
    run_gaussian_process(features, perfs, powers, areas, cross_validation_num)

    # rank boost
    # run_rank_boost(features, perfs, powers, areas, cross_validation_num)

    if args.tsne == True:
        # We want to get TSNE embedding with 2 dimensions
        n_components = 2
        tsne = TSNE(n_components)
        tsne_features = tsne.fit_transform(features)

        save_dataset(tsne_features, perfs, powers, areas, args.tsne_dataset)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dataset",
        type=str,
        default="./458.sjeng",
        help="design space dataset",
    )

    parser.add_argument(
        "--tsne",
        action="store_true",
        help="TSNE output"
    )

    parser.add_argument(
        "--tsne_dataset",
        type=str,
        default="./458.tsne.sjeng",
        help="low dimension design space dataset",
    )

    args = parser.parse_args()
    run(args)

