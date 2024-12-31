import numpy
import pandas
import scipy.stats
import torch.nn
import torch.optim

def read_params_pandas( fname : str ) -> pandas.DataFrame :
    return pandas.DataFrame( numpy.loadtxt( fname ), columns = "t x c1 c2 b a".split() ) 

def regression_vars( df: pandas.DataFrame ) -> (pandas.DataFrame, pandas.DataFrame) : 
    n = 30*60
    x = pandas.DataFrame( dict(
        x = df.a-df.b,
        y = df.x.diff(n).shift(-n) ) ).dropna()
    train = x.iloc[:x.shape[0]//2] 
    test  = x.iloc[x.shape[0]//2:] 
    return train, test


def regression(train: pandas.DataFrame, test: pandas.DataFrame) :
    r = scipy.stats.linregress( train.x, train.y )
    train_pred = r.intercept + r.slope * train.x
    test_pred  = r.intercept + r.slope * test.x
    r_train = numpy.sqrt( 1 - numpy.sum( numpy.square(train_pred - train.y) ) / numpy.sum( numpy.square( train.y ) ) )
    r_test = numpy.sqrt( 1 - numpy.sum( numpy.square(test_pred - test.y) ) / numpy.sum( numpy.square( test.y ) ) )
    return r_train, r_test, r
    

def rsquared(yp,y) : 
    return 1-torch.mean( torch.square( yp-y ) )/torch.mean( torch.square( y ) ) 



def simple_nn(train: pandas.DataFrame, test: pandas.DataFrame) :
    class SimpleNN( torch.nn.Module ) : 
        def __init__(self) : 
            super().__init__() 
            self.linear = torch.nn.Linear( 1, 1 ) 
            self.activation = torch.nn.Tanh() 
        def forward(self,x) : 
            x1 = self.linear(x)
            #print(x.shape, x1.shape, self.linear.state_dict())
            #raise Exception()
            x2 = self.activation(x1)
            return x2
    x = torch.Tensor( train.x.values ).unsqueeze(1)
    y = torch.Tensor( train.y.values ).unsqueeze(1)
    x_test = torch.Tensor( test.x.values ).unsqueeze(1)
    y_test = torch.Tensor( test.y.values ).unsqueeze(1)
    model = SimpleNN()
    optimizer = torch.optim.Adam(model.parameters())
    criterion = torch.nn.MSELoss()
    model.train()
    prev_loss = 1e9
    diff = 1e9
    while diff > 1e-14 : 
        optimizer.zero_grad()
        pred = model( x )
        #loss = (pred - y).square().mean() # criterion( pred , y )
        loss = criterion( pred , y )
        loss.backward()
        optimizer.step()
        diff = abs( prev_loss - loss.item() )
        prev_loss = loss.item()
        intercept = model.linear.state_dict()['bias'].item()
        slope = model.linear.state_dict()['weight'].item()
        rs = rsquared(pred, y).item()
        pred_test = model( x_test )
        rs_test = rsquared(pred_test, y_test).item()
        print(f"loss: {loss.item():>7f}, rs:{rs:>7f}, r:{numpy.sqrt(abs(rs)):>7f}, rst:{rs_test:>7f}, rt:{numpy.sqrt(abs(rs_test)):>7f}, diff:{diff:1.3e} slope:{slope:1.3e}, int:{intercept:1.3e}")


def simple_nn2(train: pandas.DataFrame, test: pandas.DataFrame) :
    class SimpleNN( torch.nn.Module ) : 
        def __init__(self) : 
            super().__init__() 
            self.linear1 = torch.nn.Linear( 1, 10 ) 
            self.activation1 = torch.nn.Tanh() 
            self.linear2 = torch.nn.Linear( 10, 10 ) 
            self.activation2 = torch.nn.Tanh() 
            self.linear3 = torch.nn.Linear( 10, 1 ) 
            self.activation3 = torch.nn.Tanh() 
        def forward(self,x) : 
            return self.activation3( self.linear3(
                self.activation2( self.linear2(
                    self.activation1( self.linear1( x ) ) ) ) ) )
    x = torch.Tensor( train.x.values ).unsqueeze(1)
    y = torch.Tensor( train.y.values ).unsqueeze(1)
    x_test = torch.Tensor( test.x.values ).unsqueeze(1)
    y_test = torch.Tensor( test.y.values ).unsqueeze(1)
    model = SimpleNN()
    optimizer = torch.optim.Adam(model.parameters())
    criterion = torch.nn.MSELoss()
    model.train()
    prev_loss = 1e9
    diff = 1e9
    while diff > 1e-14 : 
        optimizer.zero_grad()
        pred = model( x )
        loss = criterion( pred , y )
        loss.backward()
        optimizer.step()
        diff = abs( prev_loss - loss.item() )
        prev_loss = loss.item()
        rs = rsquared(pred, y).item()
        pred_test = model( x_test )
        rs_test = rsquared(pred_test, y_test).item()
        print(f"loss: {loss.item():>7f}, rs:{rs:>7f}, r:{numpy.sqrt(abs(rs)):>7f}, rst:{rs_test:>7f}, rt:{numpy.sqrt(abs(rs_test)):>7f}, diff:{diff:1.3e}")



def simple_nn3(train: pandas.DataFrame, test: pandas.DataFrame) :
    """
    TODO instead of the full train and test 
    """
    class SimpleNN( torch.nn.Module ) : 
        def __init__(self) : 
            super().__init__() 
            self.linear1 = torch.nn.Linear( 1, 10 ) 
            self.activation1 = torch.nn.Tanh() 
            self.linear2 = torch.nn.Linear( 10, 10 ) 
            self.activation2 = torch.nn.Tanh() 
            self.linear3 = torch.nn.Linear( 10, 1 ) 
            self.activation3 = torch.nn.Tanh() 
        def forward(self,x) : 
            return self.activation3( self.linear3(
                self.activation2( self.linear2(
                    self.activation1( self.linear1( x ) ) ) ) ) )
    x = torch.Tensor( train.x.values ).unsqueeze(1)
    y = torch.Tensor( train.y.values ).unsqueeze(1)
    x_test = torch.Tensor( test.x.values ).unsqueeze(1)
    y_test = torch.Tensor( test.y.values ).unsqueeze(1)
    model = SimpleNN()
    optimizer = torch.optim.Adam(model.parameters())
    criterion = torch.nn.MSELoss()
    model.train()
    prev_loss = 1e9
    diff = 1e9
    while diff > 1e-14 : 
        optimizer.zero_grad()
        pred = model( x )
        #loss = (pred - y).square().mean() # criterion( pred , y )
        loss = criterion( pred , y )
        loss.backward()
        optimizer.step()
        diff = abs( prev_loss - loss.item() )
        prev_loss = loss.item()
        rs = rsquared(pred, y).item()
        pred_test = model( x_test )
        rs_test = rsquared(pred_test, y_test).item()
        print(f"loss: {loss.item():>7f}, rs:{rs:>7f}, r:{numpy.sqrt(abs(rs)):>7f}, rst:{rs_test:>7f}, rt:{numpy.sqrt(abs(rs_test)):>7f}, diff:{diff:1.3e}")





def main(fname: str) : 
    df = read_params_pandas( fname )
    train, test = regression_vars( df )
    reg_train, reg_test, _ = regression( train, test ) 
    print(f"regression result: train {reg_train:1.4f}, test {reg_test:1.4f}"  )
    

